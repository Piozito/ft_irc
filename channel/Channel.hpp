#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <string>
#include <vector>
#include <algorithm>
#include "../client/Client.hpp"

class Client;
struct t_client;

class Channel {
private:
    std::string name;
    std::string topic;
    std::vector<t_client*> members;
    std::vector<t_client*> operators;
    bool inviteOnly;
    bool topicRestricted;
    std::vector<t_client*> inviteList;
    std::string key;
    size_t userLimit;

public:
    Channel(const std::string& name);
    Channel();
    ~Channel();
    Channel(const Channel& other);
    Channel &operator=(const Channel& other);

    void addMember(Client *main, t_client* client, const std::string& providedKey = "");
    void removeMember(t_client* client);
    bool isMember(const t_client* client) const;
    bool isOperator(const t_client* client) const;

    void addOperator(t_client* client);
    void removeOperator(t_client* client);

    void setTopic(const std::string& newTopic, Client *main, t_client* sender);
    void setInviteOnly(bool value);
    void setKey(const std::string& newKey);
    void setUserLimit(size_t limit);

    bool kick(Client *main, t_client* sender, t_client* target, const std::string& reason = "Kicked");
    bool invite(Client *main, t_client* sender, t_client* target);
    bool mode(t_client* sender, char flag, bool value, t_client* target = NULL, const std::string& param = "");

    const std::string& getName() const;
    const std::string& getTopic() const;
    size_t getUserCount() const;

    bool isInvited(t_client* client) const;
    void removeFromInviteList(t_client* client);
    bool isInviteOnly() const;
    bool isTopicRestricted() const;
    const std::string& getKey() const;
    size_t getUserLimit() const;
    void broadcast(Client *main, const std::string& msg, int excludeFd = -1);
    const std::vector<t_client*>& getMembers() const;
};

#endif