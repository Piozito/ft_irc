#include "Client.hpp"

Client::Client()
{
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
		this->_clients = obj._clients;
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
	
		std::string out = ":" + cli.nick + "!" + cli.user + "@localhost PRIVMSG " + target + " :" + msg;
	
		for (std::map<int, t_client>::iterator it = this->_clients.begin(); it != this->_clients.end(); ++it)
		{
			if(it->first != fd)
				sendServerMessage(it->first, out);
		}
	}
}

void Client::Register(int fd, std::string line, t_client &client)
{
	std::istringstream iss(line);
	std::string cmd;
	iss >> cmd;
	if (cmd == "NICK")
		iss >> client.nick;
	else if (cmd == "USER")
		iss >> client.user;

	if (!client.registered && !client.nick.empty() && !client.user.empty())
	{
		client.registered = true;
		sendWelcome(fd);
		std::cout << "cli [" << client.fd << "]"<< std::endl;
		std::cout << "Nickname: " << client.nick << std::endl;
		std::cout << "Username: " << client.user << std::endl;
		std::string out = ":" + this->_clients[fd].nick + "!" + this->_clients[fd].user + "@localhost PRIVMSG " + "#general" + " :" + line;
		sendServerMessage(fd, out);
	}
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
	sendServerMessage(fd, ":ft_irc 002 " + cli.nick + " :Your host is ft_irc");
	sendServerMessage(fd, ":ft_irc 003 " + cli.nick + " :This server was created 2026-03-05");

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
		
		if (!client.registered)
			Register(fd, line, client);
		else
			sendMessage(fd, line, client);
	}
}