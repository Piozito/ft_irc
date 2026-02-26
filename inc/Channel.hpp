#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <string>
#include <vector>
#include <algorithm>
#include "Client.hpp" // forward declaration works if needed

class Client;

class Channel {
private:
    std::string name;
    std::string topic;
    std::vector<Client*> members;
    std::vector<Client*> operators;
    bool inviteOnly;
    std::string key;   // channel password
    size_t userLimit;

public:
    Channel(const std::string& name);

    // Member management
    void addMember(Client* client);
    void removeMember(Client* client);
    bool isMember(Client* client) const;
    bool isOperator(Client* client) const;

    // Operator management
    void addOperator(Client* client);
    void removeOperator(Client* client);

    // Channel modes
    void setTopic(const std::string& newTopic);
    void setInviteOnly(bool value);
    void setKey(const std::string& newKey);
    void setUserLimit(size_t limit);

    // IRC commands
    bool kick(Client* sender, Client* target);       // KICK <user>
    bool invite(Client* sender, Client* target);     // INVITE <user>
    bool mode(Client* sender, char flag, bool value, Client* target = NULL); // MODE command

    // Message broadcasting
    void broadcast(const std::string& message, Client* sender);

    // Getters
    const std::string& getName() const;
    const std::string& getTopic() const;
    size_t getUserCount() const;
};

#endif