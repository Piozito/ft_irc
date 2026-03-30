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
    std::string pass;
    bool registered;
    bool pass_check;

    t_client() : fd(-1), registered(false), pass_check(false) {};
};

class Client
{
    private:
        std::map<int, t_client> _clients;  
        std::string _serverPassword;


    public:
        Client(std::string password);
        Client(const Client &copy);
        ~Client();
		Client& operator=(const Client& obj);

        void clientRead(int fd, const char* buf, int bytes);
		void sendWelcome(int fd);
		void sendServerMessage(int fd, const std::string& msg);
		void sendMessage(int fd, std::string line, t_client &cli);
		void Register(int fd, std::string line, t_client &client);
        void removeCli(int fd);
};