#include "Client.hpp"

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
        std::string chanName;
        iss >> chanName;
        if (chanName.empty() || chanName[0] != '#')
            return;

        Channel* channel = findChannelByName(cli, chanName);
        if (!channel)
        {
            cli->getChannels()[chanName] = Channel(chanName);
            channel = findChannelByName(cli, chanName);
        }

        t_client& self = cli->getClients()[fd];
        channel->addMember(cli, &self);

        std::string out = ":" + self.nick + "!" + self.user + "@localhost JOIN " + chanName;
        cli->sendServerMessage(fd, out);
        return;
    }

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

        if (target.empty() || target[0] != '#')
            return;

        Channel* channel = findChannelByName(cli, target);
        if (!channel)
        {
            cli->sendServerMessage(fd, "403 " + target + " :No such channel");
            return;
        }

        const std::map<int, t_client>& clients = cli->getClients();
        std::map<int, t_client>::const_iterator It = clients.find(fd);
        if (It == clients.end())
            return;

        std::string out = ":" + It->second.nick + "!" + It->second.user +
                          "@localhost PRIVMSG " + channel->getName() + " :" + msg;

        for (std::map<int, t_client>::const_iterator it = clients.begin(); it != clients.end(); ++it)
        {
            if (it->first != fd)
                cli->sendServerMessage(it->first, out);
        }
    }
    else if (cmd == "KICK")
    {
        std::string chanName, target;
        iss >> chanName >> target;
        if (chanName.empty() || target.empty())
            return;

        Channel* channel = findChannelByName(cli, chanName);
        if (!channel)
            return;

        const std::map<int, t_client>& clients = cli->getClients();
        std::map<int, t_client>::const_iterator It = clients.find(fd);
        if (It == clients.end())
            return;

        if (channel->isOperator(&It->second))
        {
            int targetFd = findClient(cli, target);
            if (targetFd != -1)
                channel->kick(cli, &cli->getClients()[fd], &cli->getClients()[targetFd]);
            else
                cli->sendServerMessage(fd, "Target not found.");
        }
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
            sendServerMessage(fd, "ERROR :Closing Link: Password required");
            return false;
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
        if (cli.pass.empty())
        {
            sendServerMessage(fd, ":ft_irc 464 * :Password required");
            sendServerMessage(fd, "ERROR :Closing Link: Password required");
            return false;
        }
        if (cmd == "NICK")
        {
            if (!(iss >> cli.nick))
            {
                sendServerMessage(fd, ":ft_irc 461 NICK :Not enough parameters");
                return true;
            }
        }
        else if(cmd == "USER")
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

    if (!cli.registered &&
        !cli.pass.empty() &&
        !cli.nick.empty() &&
        !cli.user.empty())
    {
        cli.registered = true;
        this->_channels["#general"].addMember(this, &cli, "");
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
        }
    }
    return true;
}

void Client::removeCli(int fd)
{
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

    sendServerMessage(fd, ":ft_irc 001 " + cli.nick + " :Welcome to ft_irc!");
    sendServerMessage(fd, ":ft_irc 375 " + cli.nick + " :- Welcome to ft_irc " +  cli.nick +"! -");
    sendServerMessage(fd, ":ft_irc 372 " + cli.nick + " :- Created by fragarc2, mde-maga and aaleixo- -");
    sendServerMessage(fd, ":ft_irc 376 " + cli.nick + " :End of MOTD");
}