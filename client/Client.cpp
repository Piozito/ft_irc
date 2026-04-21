#include "Client.hpp"
#include <cctype>

static bool isValidNick(const std::string& nick)
{
    if (nick.empty() || nick.size() > 9)
        return false;
    if (!std::isalpha((unsigned char)nick[0]) && nick[0] != '_')
        return false;
    for (size_t i = 1; i < nick.size(); ++i)
    {
        char c = nick[i];
        if (!std::isalnum((unsigned char)c) && c != '-' && c != '_' &&
            c != '[' && c != ']' && c != '\\' && c != '^' && c != '{' && c != '}' && c != '|')
            return false;
    }
    return true;
}

static bool isNickInUse(Client* cli, const std::string& nick, int excludeFd)
{
    const std::map<int, t_client>& clients = cli->getClients();
    for (std::map<int, t_client>::const_iterator it = clients.begin(); it != clients.end(); ++it)
    {
        if (it->first != excludeFd && it->second.nick == nick)
            return true;
    }
    return false;
}

t_client::t_client()
{
    this->fd = -1;
    this->registered = false;
    this->pass = "";
    this->nick = "";
    this->user = "";
    this->buf = "";
}

Client::Client(std::string password)
{
    this->_serverPassword = password;
    this->_channels["#general"] = Channel("#general");
}

Client::Client(const Client &copy)
{
    *this = copy;
}

Client::~Client()
{
}

Client& Client::operator=(const Client& obj)
{
    if(this != &obj)
    {
        this->_clients = obj._clients;
        this->_serverPassword = obj._serverPassword;
        this->_channels = obj._channels;
    }
    return *this;
}

std::string getNickname(t_client *cli)
{
    return cli->nick;
}

std::map<int, t_client>& Client::getClients()
{
    return this->_clients;
}

const std::map<int, t_client>& Client::getClients() const
{
    return this->_clients;
}

std::map<std::string, Channel>& Client::getChannels()
{
    return this->_channels;
}

const std::map<std::string, Channel>& Client::getChannels() const
{
    return this->_channels;
}

static Channel* findChannelByName(Client* cli, const std::string& name)
{
    std::map<std::string, Channel>& chans = cli->getChannels();
    std::map<std::string, Channel>::iterator it = chans.find(name);
    if (it == chans.end())
        return NULL;
    return &it->second;
}

void sendMessage(int fd, std::string line, Client *cli)
{
    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;

    if (cmd.empty())
        return;

    if (cmd[0] == ':')
    {
        if (!(iss >> cmd))
            return;
    }

    if (cmd == "JOIN")
    {
        std::string chanName, key;
        iss >> chanName >> key;
        if (chanName.empty() || chanName[0] != '#')
            return;

        Channel* channel = findChannelByName(cli, chanName);
        bool isNew = (channel == NULL);
        if (!channel)
        {
            cli->getChannels()[chanName] = Channel(chanName);
            channel = findChannelByName(cli, chanName);
        }

        t_client& self = cli->getClients()[fd];
        channel->addMember(cli, &self, key);

        if (!channel->isMember(&self))
            return;

        if (isNew)
            channel->addOperator(&self);

        std::string out = ":" + self.nick + "!" + self.user + "@localhost JOIN " + chanName;
        channel->broadcast(cli, out);

        if (!channel->getTopic().empty())
            cli->sendServerMessage(fd, ":ft_irc 332 " + self.nick + " " + chanName + " :" + channel->getTopic());
        return;
    }

    const std::map<int, t_client>& clients = cli->getClients();
    std::map<int, t_client>::const_iterator It = clients.find(fd);
    if (It == clients.end())
        return;
    const t_client& self = It->second;

    if (cmd == "PRIVMSG")
    {
        std::string target;
        iss >> target;

        std::string msg;
        std::getline(iss, msg);
        if (!msg.empty() && msg[0] == ' ')
            msg.erase(0, 1);
        if (!msg.empty() && msg[0] == ':')
            msg.erase(0, 1);

        std::string out = ":" + self.nick + "!" + self.user + "@localhost PRIVMSG " + target + " :" + msg;

        if (!target.empty() && target[0] == '#')
        {
            Channel* channel = findChannelByName(cli, target);
            if (!channel)
            {
                cli->sendServerMessage(fd, ":ft_irc 403 " + self.nick + " " + target + " :No such channel");
                return;
            }
            if (!channel->isMember(&self))
            {
                cli->sendServerMessage(fd, ":ft_irc 404 " + self.nick + " " + target + " :Cannot send to channel");
                return;
            }
            channel->broadcast(cli, out, fd);
        }
        else
        {
            int targetFd = findClient(cli, target);
            if (targetFd == -1)
            {
                cli->sendServerMessage(fd, ":ft_irc 401 " + self.nick + " " + target + " :No such nick/channel");
                return;
            }
            cli->sendServerMessage(targetFd, out);
        }
    }
    else if (cmd == "KICK")
    {
        std::string chanName, target, reason;
        iss >> chanName >> target;
        std::getline(iss, reason);
        if (!reason.empty() && reason[0] == ' ') reason.erase(0, 1);
        if (!reason.empty() && reason[0] == ':') reason.erase(0, 1);
        if (reason.empty()) reason = "Kicked";

        if (chanName.empty() || target.empty())
            return;

        Channel* channel = findChannelByName(cli, chanName);
        if (!channel)
        {
            cli->sendServerMessage(fd, ":ft_irc 403 " + self.nick + " " + chanName + " :No such channel");
            return;
        }

        if (!channel->isOperator(&self))
        {
            cli->sendServerMessage(fd, ":ft_irc 482 " + self.nick + " " + chanName + " :You're not channel operator");
            return;
        }

        int targetFd = findClient(cli, target);
        if (targetFd == -1)
        {
            cli->sendServerMessage(fd, ":ft_irc 401 " + self.nick + " " + target + " :No such nick");
            return;
        }
        channel->kick(cli, &cli->getClients()[fd], &cli->getClients()[targetFd]);
    }
    else if (cmd == "INVITE")
    {
        std::string targetNick, chanName;
        iss >> targetNick >> chanName;

        if (targetNick.empty() || chanName.empty())
        {
            cli->sendServerMessage(fd, ":ft_irc 461 INVITE :Not enough parameters");
            return;
        }

        Channel* channel = findChannelByName(cli, chanName);
        if (!channel)
        {
            cli->sendServerMessage(fd, ":ft_irc 403 " + self.nick + " " + chanName + " :No such channel");
            return;
        }

        if (!channel->isOperator(&self))
        {
            cli->sendServerMessage(fd, ":ft_irc 482 " + self.nick + " " + chanName + " :You're not channel operator");
            return;
        }

        int targetFd = findClient(cli, targetNick);
        if (targetFd == -1)
        {
            cli->sendServerMessage(fd, ":ft_irc 401 " + self.nick + " " + targetNick + " :No such nick");
            return;
        }

        t_client& target = cli->getClients()[targetFd];
        if (channel->invite(cli, &cli->getClients()[fd], &target))
            cli->sendServerMessage(targetFd, ":" + self.nick + "!" + self.user + "@localhost INVITE " + targetNick + " :" + chanName);
    }
    else if (cmd == "TOPIC")
    {
        std::string chanName;
        iss >> chanName;

        if (chanName.empty())
        {
            cli->sendServerMessage(fd, ":ft_irc 461 TOPIC :Not enough parameters");
            return;
        }

        Channel* channel = findChannelByName(cli, chanName);
        if (!channel)
        {
            cli->sendServerMessage(fd, ":ft_irc 403 " + self.nick + " " + chanName + " :No such channel");
            return;
        }

        std::string rest;
        std::getline(iss, rest);
        while (!rest.empty() && rest[0] == ' ') rest.erase(0, 1);

        if (rest.empty())
        {
            if (channel->getTopic().empty())
                cli->sendServerMessage(fd, ":ft_irc 331 " + self.nick + " " + chanName + " :No topic is set");
            else
                cli->sendServerMessage(fd, ":ft_irc 332 " + self.nick + " " + chanName + " :" + channel->getTopic());
        }
        else
        {
            if (rest[0] == ':') rest.erase(0, 1);
            channel->setTopic(rest, cli, &cli->getClients()[fd]);
            std::string out = ":" + self.nick + "!" + self.user + "@localhost TOPIC " + chanName + " :" + rest;
            channel->broadcast(cli, out);
        }
    }
    else if (cmd == "MODE")
    {
        std::string chanName, modeStr;
        iss >> chanName >> modeStr;

        if (chanName.empty())
            return;

        Channel* channel = findChannelByName(cli, chanName);
        if (!channel)
        {
            cli->sendServerMessage(fd, ":ft_irc 403 " + self.nick + " " + chanName + " :No such channel");
            return;
        }

        if (modeStr.empty())
        {
            std::string modes = "+";
            if (channel->isInviteOnly()) modes += "i";
            if (channel->isTopicRestricted()) modes += "t";
            if (!channel->getKey().empty()) modes += "k";
            if (channel->getUserLimit() > 0) modes += "l";
            cli->sendServerMessage(fd, ":ft_irc 324 " + self.nick + " " + chanName + " " + modes);
            return;
        }

        bool value = (modeStr[0] != '-');
        size_t flagStart = (modeStr[0] == '+' || modeStr[0] == '-') ? 1 : 0;

        std::string appliedModes = (value ? "+" : "-");
        std::string appliedParams;

        for (size_t i = flagStart; i < modeStr.size(); ++i)
        {
            char flag = modeStr[i];
            std::string param;
            t_client* target = NULL;

            if ((flag == 'k' && value) || (flag == 'l' && value))
                iss >> param;
            else if (flag == 'o')
            {
                std::string targetNick;
                iss >> targetNick;
                int targetFd = findClient(cli, targetNick);
                if (targetFd != -1)
                {
                    target = &cli->getClients()[targetFd];
                    param = targetNick;
                }
            }

            if (channel->mode(&cli->getClients()[fd], flag, value, target, param))
            {
                appliedModes += flag;
                if (!param.empty())
                    appliedParams += " " + param;
            }
        }

        if (appliedModes.size() > 1)
        {
            std::string out = ":" + self.nick + "!" + self.user + "@localhost MODE " + chanName + " " + appliedModes + appliedParams;
            channel->broadcast(cli, out);
        }
    }
    else if (cmd == "NICK")
    {
        std::string newNick;
        iss >> newNick;

        if (newNick.empty())
        {
            cli->sendServerMessage(fd, ":ft_irc 431 " + self.nick + " :No nickname given");
            return;
        }
        if (!isValidNick(newNick))
        {
            cli->sendServerMessage(fd, ":ft_irc 432 " + self.nick + " " + newNick + " :Erroneous nickname");
            return;
        }
        if (isNickInUse(cli, newNick, fd))
        {
            cli->sendServerMessage(fd, ":ft_irc 433 " + self.nick + " " + newNick + " :Nickname is already in use");
            return;
        }
        std::string out = ":" + self.nick + "!" + self.user + "@localhost NICK :" + newNick;
        cli->getClients()[fd].nick = newNick;
        cli->sendServerMessage(fd, out);
    }
    else if (cmd == "PART")
    {
        std::string chanName, reason;
        iss >> chanName;
        std::getline(iss, reason);
        if (!reason.empty() && reason[0] == ' ') reason.erase(0, 1);
        if (!reason.empty() && reason[0] == ':') reason.erase(0, 1);

        Channel* channel = findChannelByName(cli, chanName);
        if (!channel)
            return;

        std::string out = ":" + self.nick + "!" + self.user + "@localhost PART " + chanName;
        if (!reason.empty()) out += " :" + reason;
        channel->broadcast(cli, out);
        channel->removeMember(&cli->getClients()[fd]);
    }
    else if (cmd == "HELP")
    {
        cli->sendServerMessage(fd, ":ft_irc NOTICE " + self.nick + " :=== ft_irc HELP ===");
        cli->sendServerMessage(fd, ":ft_irc NOTICE " + self.nick + " :--- Connection ---");
        cli->sendServerMessage(fd, ":ft_irc NOTICE " + self.nick + " :  PASS <password>          Server password (required first)");
        cli->sendServerMessage(fd, ":ft_irc NOTICE " + self.nick + " :  NICK <nickname>          Set or change your nickname");
        cli->sendServerMessage(fd, ":ft_irc NOTICE " + self.nick + " :  USER <user> 0 * :<name>  Set your username");
        cli->sendServerMessage(fd, ":ft_irc NOTICE " + self.nick + " :  QUIT [reason]            Disconnect from the server");
        cli->sendServerMessage(fd, ":ft_irc NOTICE " + self.nick + " :--- Channels ---");
        cli->sendServerMessage(fd, ":ft_irc NOTICE " + self.nick + " :  JOIN #channel [key]      Join a channel (use key if +k is set)");
        cli->sendServerMessage(fd, ":ft_irc NOTICE " + self.nick + " :  PART #channel [reason]   Leave a channel");
        cli->sendServerMessage(fd, ":ft_irc NOTICE " + self.nick + " :  TOPIC #channel [topic]   View or set the channel topic");
        cli->sendServerMessage(fd, ":ft_irc NOTICE " + self.nick + " :--- Messaging ---");
        cli->sendServerMessage(fd, ":ft_irc NOTICE " + self.nick + " :  PRIVMSG #channel :msg    Send message to a channel");
        cli->sendServerMessage(fd, ":ft_irc NOTICE " + self.nick + " :  PRIVMSG nickname :msg    Send private message to a user");
        cli->sendServerMessage(fd, ":ft_irc NOTICE " + self.nick + " :--- Operator only ---");
        cli->sendServerMessage(fd, ":ft_irc NOTICE " + self.nick + " :  KICK #channel nick       Remove a user from a channel");
        cli->sendServerMessage(fd, ":ft_irc NOTICE " + self.nick + " :  INVITE nick #channel     Invite a user to a channel");
        cli->sendServerMessage(fd, ":ft_irc NOTICE " + self.nick + " :  MODE #channel +i         Set invite-only");
        cli->sendServerMessage(fd, ":ft_irc NOTICE " + self.nick + " :  MODE #channel +t         Restrict TOPIC to operators");
        cli->sendServerMessage(fd, ":ft_irc NOTICE " + self.nick + " :  MODE #channel +k <key>   Set channel password");
        cli->sendServerMessage(fd, ":ft_irc NOTICE " + self.nick + " :  MODE #channel +o nick    Give operator to user");
        cli->sendServerMessage(fd, ":ft_irc NOTICE " + self.nick + " :  MODE #channel +l <n>     Set max users in channel");
        cli->sendServerMessage(fd, ":ft_irc NOTICE " + self.nick + " :  (use - instead of + to unset any mode)");
        cli->sendServerMessage(fd, ":ft_irc NOTICE " + self.nick + " :===================");
    }
    else if (cmd == "QUIT")
    {
        std::string reason;
        std::getline(iss, reason);
        if (!reason.empty() && reason[0] == ' ') reason.erase(0, 1);
        if (!reason.empty() && reason[0] == ':') reason.erase(0, 1);
        if (reason.empty()) reason = "Leaving";

        std::string out = ":" + self.nick + "!" + self.user + "@localhost QUIT :" + reason;
        for (std::map<int, t_client>::const_iterator it = clients.begin(); it != clients.end(); ++it)
        {
            if (it->first != fd)
                cli->sendServerMessage(it->first, out);
        }
        cli->sendServerMessage(fd, "ERROR :Closing Link: " + reason);
    }
}

int findClient(Client *main, std::string name)
{
    const std::map<int, t_client>& clients = main->getClients();
    for(std::map<int, t_client>::const_iterator i = clients.begin(); i != clients.end(); i++)
    {
        if(i->second.nick == name)
            return i->second.fd;
    }
    return -1;
}

bool Client::Register(int fd, std::string line, t_client &cli)
{
    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;

    if (cmd.empty())
        return true;

    if (cmd == "PASS")
    {
        if (!cli.pass.empty())
        {
            sendServerMessage(fd, ":ft_irc 462 * :You may not reregister");
            return true;
        }

        std::string providedPass;
        if (!(iss >> providedPass))
        {
            sendServerMessage(fd, ":ft_irc 461 PASS :Not enough parameters");
            return true;
        }

        if (!providedPass.empty() && providedPass[0] == ':')
            providedPass.erase(0, 1);

        if (providedPass != this->_serverPassword)
        {
            sendServerMessage(fd, ":ft_irc 464 * :Password incorrect");
            sendServerMessage(fd, "ERROR :Closing Link: Password incorrect");
            return false;
        }

        cli.pass = providedPass;
    }
    else if (cmd == "NICK" || cmd == "USER")
    {
        // PASS must come before NICK/USER
        if (cli.pass.empty())
        {
            sendServerMessage(fd, ":ft_irc 464 * :Password required");
            sendServerMessage(fd, "ERROR :Closing Link: Password required");
            return false;
        }

        // If already registered, client can't change NICK/USER
        if (cli.registered)
        {
            sendServerMessage(fd, ":ft_irc 462 * :You may not reregister");
            return true;
        }

        if (cmd == "NICK")
        {
            std::string newNick;
            if (!(iss >> newNick))
            {
                sendServerMessage(fd, ":ft_irc 431 * :No nickname given");
                return true;
            }
            if (!isValidNick(newNick))
            {
                sendServerMessage(fd, ":ft_irc 432 " + newNick + " :Erroneous nickname");
                return true;
            }
            if (isNickInUse(this, newNick, fd))
            {
                sendServerMessage(fd, ":ft_irc 433 * " + newNick + " :Nickname is already in use");
                return true;
            }
            cli.nick = newNick;
        }
        else if (cmd == "USER")
        {
            if (!(iss >> cli.user))
            {
                sendServerMessage(fd, ":ft_irc 461 USER :Not enough parameters");
                return true;
            }
        }
    }
    else
    {
        sendServerMessage(fd, ":ft_irc 451 * :You have not registered");
        return true;
    }

    // Register only if pass, nick, user are all set and client isn’t already registered
    if (!cli.registered &&
        !cli.pass.empty() &&
        !cli.nick.empty() &&
        !cli.user.empty())
    {
        cli.registered = true;
        bool isFirstMember = (this->_channels["#general"].getUserCount() == 0);
        this->_channels["#general"].addMember(this, &cli, "");
        if (isFirstMember)
            this->_channels["#general"].addOperator(&cli);
        sendWelcome(fd);
    }

    return true;
}

bool Client::clientRead(int fd, const char* buf, int bytes)
{
    t_client& client = this->_clients[fd];
    client.fd = fd;

    client.buf.append(buf, bytes);

    while (true)
    {
        std::string::size_type pos = client.buf.find("\r\n");
        size_t skip = 2;

        if (pos == std::string::npos)
        {
            pos = client.buf.find("\n");
            skip = 1;
        }
        if (pos == std::string::npos)
            break;

        std::string line = client.buf.substr(0, pos);
        client.buf.erase(0, pos + skip);

        if (line.rfind("PING", 0) == 0)
        {
            std::string token = line.substr(4);
            while (!token.empty() && (token[0] == ' ' || token[0] == ':'))
                token.erase(0, 1);

            sendServerMessage(fd, ":ft_irc PONG ft_irc :" + token);
            continue;
        }

        if (line.rfind("CAP LS", 0) == 0)
        {
            sendServerMessage(fd, "CAP * LS :");
            sendServerMessage(fd, "CAP * END");
            continue;
        }

        if (!client.registered)
        {
            if (!Register(fd, line, client))
                return false;
        }
        else
        {
            sendMessage(fd, line, this);
            if (line.rfind("QUIT", 0) == 0)
                return false;
        }
    }
    return true;
}

void Client::removeCli(int fd)
{
    std::map<int, t_client>::iterator it = this->_clients.find(fd);
    if (it == this->_clients.end())
        return;
    t_client* client = &it->second;
    for (std::map<std::string, Channel>::iterator ch = this->_channels.begin(); ch != this->_channels.end(); ++ch)
        ch->second.removeMember(client);
    this->_clients.erase(fd);
}

void Client::sendServerMessage(int fd, const std::string& msg)
{
    std::string res = msg + "\r\n";
    send(fd, res.c_str(), res.size(), 0);
}

void Client::sendWelcome(int fd)
{
    t_client &cli = this->_clients[fd];

    sendServerMessage(fd, ":ft_irc 001 " + cli.nick + " :Welcome to ft_irc, " + cli.nick + "!");
    sendServerMessage(fd, ":ft_irc 375 " + cli.nick + " :- ft_irc Message of the Day -");
    sendServerMessage(fd, ":ft_irc 372 " + cli.nick + " :- Created by fragarc2, mde-maga and aaleixo-");
    sendServerMessage(fd, ":ft_irc 372 " + cli.nick + " :-");
    sendServerMessage(fd, ":ft_irc 372 " + cli.nick + " :- HOW TO GET STARTED");
    sendServerMessage(fd, ":ft_irc 372 " + cli.nick + " :-   1. You are already registered as " + cli.nick);
    sendServerMessage(fd, ":ft_irc 372 " + cli.nick + " :-   2. Join a channel:  JOIN #channel");
    sendServerMessage(fd, ":ft_irc 372 " + cli.nick + " :-   3. Send a message:  PRIVMSG #channel :your message");
    sendServerMessage(fd, ":ft_irc 372 " + cli.nick + " :-   4. Message a user:  PRIVMSG nickname :your message");
    sendServerMessage(fd, ":ft_irc 372 " + cli.nick + " :-");
    sendServerMessage(fd, ":ft_irc 372 " + cli.nick + " :- You have been auto-joined to #general");
    sendServerMessage(fd, ":ft_irc 372 " + cli.nick + " :- Type HELP to see all available commands");
    sendServerMessage(fd, ":ft_irc 376 " + cli.nick + " :End of MOTD");
}