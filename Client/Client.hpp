#pragma once

#include <sys/socket.h>
#include <iostream>
#include <sstream>
#include <string>
#include <map>

struct t_client
{
    int fd;
    std::string buf;
    std::string user;
    std::string nick;
    bool registered;

    t_client() : fd(-1), registered(false) {};
};

class Client
{
    private:
        std::map<int, t_client> _clients;  


    public:
        Client();
        Client(const Client &copy);
        ~Client();
		Client& operator=(const Client& obj);

        void clientRead(int fd, const char* buf, int bytes);
		void sendWelcome(int fd);
		void sendServerMessage(int fd, const std::string& msg);
		void sendMessage(int fd, std::string line, t_client &cli);
		void Register(int fd, std::string line, t_client &client);
};