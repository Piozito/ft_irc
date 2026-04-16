#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <string>
#include <vector>
#include <algorithm>
#include "Client.hpp"

class Client;

class Channel {
private:
    std::string name;
    std::string topic;
    std::vector<Client*> members;
    std::vector<Client*> operators;
    bool inviteOnly;
    bool topicRestricted;
    std::vector<Client*> inviteList;
    std::string key;
    size_t userLimit;

public:
    Channel(const std::string& name);

    void addMember(Client* client, const std::string& providedKey = "");
    void removeMember(Client* client);
    bool isMember(Client* client) const;
    bool isOperator(Client* client) const;

    void addOperator(Client* client);
    void removeOperator(Client* client);

    void setTopic(const std::string& newTopic, Client* sender);
    void setInviteOnly(bool value);
    void setKey(const std::string& newKey);
    void setUserLimit(size_t limit);

    bool kick(Client* sender, Client* target);
    bool invite(Client* sender, Client* target);
    bool mode(Client* sender, char flag, bool value, Client* target = NULL, const std::string& param = "");

    void broadcast(const std::string& message, Client* sender);

    const std::string& getName() const;
    const std::string& getTopic() const;
    size_t getUserCount() const;

    bool isInvited(Client* client) const;
    void removeFromInviteList(Client* client);
    bool isInviteOnly() const;
    bool isTopicRestricted() const;
    const std::string& getKey() const;
    size_t getUserLimit() const;
};

#endif