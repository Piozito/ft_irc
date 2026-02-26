/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: fragarc2 <fragarc2@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/26 15:27:41 by fragarc2          #+#    #+#             */
/*   Updated: 2026/02/26 16:34:34 by fragarc2         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef Server_HPP
#define Server_HPP

#include <iostream>
#include <cstdlib>
#include <limits>
#include <algorithm>
#include <fstream>
#include <fcntl.h>
#include <sstream>
#include <map>
#include <vector>
#include <string>
#include <cstring>
#include <poll.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#define INT_MAX std::numeric_limits<int>::max()

class Server
{
	public:
		Server(int port);
		~Server();
		void serverer();

	private:
		Server(const Server&);
		Server& operator=(const Server& obj);

		int _serverSocket;
		std::vector<pollfd> _fds;
		int _port;
};

#endif
