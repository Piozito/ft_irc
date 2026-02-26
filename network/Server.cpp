/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: fragarc2 <fragarc2@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/26 12:03:24 by fragarc2          #+#    #+#             */
/*   Updated: 2026/02/26 17:15:40 by fragarc2         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"

Server::Server(int port)
{
	_port = port;
	_serverSocket = -1;
}
Server::~Server()
{
		if (_serverSocket != -1)
		close(_serverSocket);
}

void Server::serverer()
{
	std::vector<pollfd> fds;

	_serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (_serverSocket < 0)
		throw std::runtime_error("socket() failed");

	int flags = fcntl(_serverSocket, F_GETFL, 0);
	if (flags == -1 || fcntl(_serverSocket, F_SETFL, flags | O_NONBLOCK) == -1)
		throw std::runtime_error("fcntl() failed");

	sockaddr_in serverAddress;
	std::memset(&serverAddress, 0, sizeof(serverAddress));
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(_port);
	serverAddress.sin_addr.s_addr = INADDR_ANY;

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
			 throw std::runtime_error("poll() failed");

		for (size_t i = 0; i < fds.size(); ++i)
		{
		if (fds[i].revents & (POLLHUP | POLLERR))
		{
			close(fds[i].fd);
			fds.erase(fds.begin() + i);
			--i;
			continue;
		}

			if (fds[i].revents & POLLIN)
			{
				if (fds[i].fd == _serverSocket)
				{
					int clientSocket = accept(_serverSocket, nullptr, nullptr);
					if (clientSocket < 0)
						continue;
					int cflags = fcntl(clientSocket, F_GETFL, 0);
					if (cflags != -1)
						fcntl(clientSocket, F_SETFL, cflags | O_NONBLOCK);
					pollfd clientFd;
					clientFd.fd = clientSocket;
					clientFd.events = POLLIN;
					clientFd.revents = 0;
					fds.push_back(clientFd);
					std::cout << "New penis connected!" << std::endl;
				}
				else
				{
					char buffer[1024] = {0};
					int data = recv(fds[i].fd, buffer, sizeof(buffer) - 1, 0);
					if(data <= 0)
					{
						close(fds[i].fd);
						fds.erase(fds.begin() + i);
						--i;
					}
					else
					{
						buffer[data] = '\0';
						std::cout << "Message from penis: " << buffer << std::endl;

					}
				}
			}
		}
	}
}

int main(int argc, char **argv)
{
	if (argc != 2)
	{
		std::cerr << "Usage: " << argv[0] << " <port>" << std::endl;
		return 1;
	}

	int port = atoi(argv[1]);
	if (port <= 0 || port > 65535)
	{
		std::cerr << "Invalid port number" << std::endl;
		return 1;
	}

	try {
		Server server(port);
		server.serverer();
	}
	catch (const std::exception& e) {
		std::cerr << "Server error: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
