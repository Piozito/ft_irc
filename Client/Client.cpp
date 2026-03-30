#include "Client.hpp"

Client::Client(std::string password)
{
	this->_serverPassword = password;
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
	}
	return *this;
}

void Client::sendMessage(int fd, std::string line, t_client &cli)
{
	std::istringstream iss(line);
	std::string cmd;
	iss >> cmd;

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
		if (target[0] != '#')
			return;
		
		std::string out = ":" + cli.nick + "!" + cli.user + "@localhost PRIVMSG " + target + " :" + msg;
	
		for (std::map<int, t_client>::iterator it = this->_clients.begin(); it != this->_clients.end(); ++it)
		{
			if(it->first != fd)
				sendServerMessage(it->first, out);
		}
	}
}

void Client::Register(int fd, std::string line, t_client &cli)
{
	std::istringstream iss(line);
	std::string cmd;
	iss >> cmd;

	if (cmd == "PASS")
	{
		if (!(iss >> cli.pass) && cli.pass != this->_serverPassword)
		{
			sendServerMessage(fd, ":ft_irc 464 * :Password incorrect");
			sendServerMessage(fd, "ERROR :Password incorrect");
			return;
		}
		return;
	}
	else if (cmd == "NICK")
		iss >> cli.nick;
	else if (cmd == "USER")
		iss >> cli.user;

	if (!cli.registered &&
		!cli.pass.empty() &&
		!cli.nick.empty() &&
		!cli.user.empty())
	{
		cli.registered = true;
		sendWelcome(fd);
	}
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

	// obriga a entrar no #general (remover quando houver os channels como deve ser)
	sendServerMessage(fd, ":" + cli.nick + "!" + cli.user + "@localhost JOIN #general");
}

void Client::clientRead(int fd, const char* buf, int bytes)
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
		
		std::cout << line << std::endl;

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
			Register(fd, line, client);
		else
			sendMessage(fd, line, client);
	}
}