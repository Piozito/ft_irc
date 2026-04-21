#pragma once

#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <string>
#include <map>

#include "../channel/Channel.hpp"

struct t_client
{
    int fd;
    std::string buf;
    std::string user;
    std::string nick;
    std::string pass;
    bool registered;

    t_client();
};

class Channel;

class Client
{
    private:
        std::map<int, t_client> _clients;
        std::map<std::string, Channel> _channels;
        std::string _serverPassword;

    public:
        Client(std::string password);
        Client(const Client &copy);
        ~Client();
        Client& operator=(const Client& obj);

        const std::map<int, t_client>& getClients() const;
        std::map<int, t_client>& getClients();
        const std::map<std::string, Channel>& getChannels() const;
        std::map<std::string, Channel>& getChannels();

        bool clientRead(int fd, const char* buf, int bytes);
        void sendWelcome(int fd);
        void sendServerMessage(int fd, const std::string& msg);
        bool Register(int fd, std::string line, t_client &client);
        void removeCli(int fd);
};

void sendMessage(int fd, std::string line, Client *cli);
int findClient(Client *main, std::string name);
std::string getNickname(t_client *cli);