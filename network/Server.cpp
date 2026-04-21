/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: aaleixo- <aaleixo-@student.42lisboa.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/26 12:03:24 by fragarc2          #+#    #+#             */
/*   Updated: 2026/04/20 07:32:43 by aaleixo-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"

Server::Server(int port, std::string password) : _cli(password)
{
	_port = port;
	_password = password;
	_serverSocket = -1;
}
Server::~Server()
{
	if (_serverSocket != -1)
		close(_serverSocket);
}

Server& Server::operator=(const Server& obj)
{
	if(this != &obj)
	{
		this->_port = obj._port;
		this->_serverSocket = obj._serverSocket;
		this->_fds = obj._fds;
	}
	return *this;
}

void Server::serverer()
{
	std::vector<pollfd> fds;

	_serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (_serverSocket < 0)
		throw std::runtime_error("socket() failed");

	if (fcntl(_serverSocket, F_SETFL, O_NONBLOCK) == -1) //CAN ONLY USE O_NONBLOCK AS A FLAG!
		throw std::runtime_error("fcntl() failed");

	sockaddr_in serverAddress;
	std::memset(&serverAddress, 0, sizeof(serverAddress));
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(_port);
	serverAddress.sin_addr.s_addr = INADDR_ANY;

	int opt = 1;
	if (setsockopt(_serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
		throw std::runtime_error("setsockopt(SO_REUSEADDR) failed");


	if (bind(_serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0)
		throw std::runtime_error("bind() failed");

	if (listen(_serverSocket, 5) < 0)
		throw std::runtime_error("listen() failed");

	pollfd serverFd;
	serverFd.fd = _serverSocket;
	serverFd.events = POLLIN;
	serverFd.revents = 0;
	fds.push_back(serverFd);

	while (true)
	{
		if (poll(fds.data(), fds.size(), -1) < 0)
		{
			if (errno == EINTR) continue;
			throw std::runtime_error("poll() failed");
		}

		for (size_t i = 0; i < fds.size(); ++i)
		{
		if (fds[i].revents & (POLLHUP | POLLERR))
		{
			this->_cli.removeCli(fds[i].fd);
			close(fds[i].fd);
			fds.erase(fds.begin() + i);
			--i;
			continue;
		}

			if (fds[i].revents & POLLIN)
			{
				if (fds[i].fd == _serverSocket)
				{
					int clientSocket = accept(_serverSocket, NULL, NULL);
					if (clientSocket < 0)
						continue;
					fcntl(clientSocket, F_SETFL, O_NONBLOCK);
					pollfd clientFd;
					clientFd.fd = clientSocket;
					clientFd.events = POLLIN;
					clientFd.revents = 0;
					fds.push_back(clientFd);
					std::cout << "New user connected!" << std::endl;
				}
				else
				{
					char buffer[1024] = {0};
					int data = recv(fds[i].fd, buffer, sizeof(buffer) - 1, 0);
					if(data <= 0)
					{
						std::cout  << "User disconnected" << std::endl;
						this->_cli.removeCli(fds[i].fd);
						close(fds[i].fd);
						fds.erase(fds.begin() + i);
						--i;
					}
					else
					{
						buffer[data] = '\0';

						std::cout << buffer << std::endl;

						if(!this->_cli.clientRead(fds[i].fd, buffer, data))
						{
							std::cout  << "User disconnected" << std::endl;
							this->_cli.removeCli(fds[i].fd);
							close(fds[i].fd);
							fds.erase(fds.begin() + i);
							--i;
						}
					}
				}
			}
		}
	}
}
