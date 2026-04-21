#include "Channel.hpp"
#include <iostream>
#include <cstdlib>

Channel::Channel(const std::string& name)
    : name(name), topic(""), inviteOnly(false), topicRestricted(false), key(""), userLimit(0) {}

Channel::Channel()
{
    this->name = "#general";
    this->topic = "";
    this->inviteOnly = false;
    this->topicRestricted = false;
    this->key = "";
    this->userLimit = 0;
}

Channel::~Channel()
{

}

Channel::Channel(const Channel& other)
{
    *this = other;
}

Channel &Channel::operator=(const Channel& other)
{
    if(this != &other)
    {
        this->name = other.name;
        this->topic = other.topic;
        this->inviteOnly = other.inviteOnly;
        this->topicRestricted = other.topicRestricted;
        this->key = other.key;
        this->userLimit = other.userLimit;
    }
    return *this;
}

// -------------------- Members --------------------
void Channel::addMember(Client *main, t_client* client, const std::string& providedKey) {
    if (isMember(client))
        return;

    if (inviteOnly && !isInvited(client)) {
        main->sendServerMessage(client->fd, ":ft_irc 473 " + getNickname(client) + " " + name + " :Cannot join channel (+i)");
        return;
    }

    if (!key.empty() && providedKey != key) {
        main->sendServerMessage(client->fd, ":ft_irc 475 " + getNickname(client) + " " + name + " :Cannot join channel (+k)");
        return;
    }

    if (userLimit > 0 && members.size() >= userLimit) {
        main->sendServerMessage(client->fd, ":ft_irc 471 " + getNickname(client) + " " + name + " :Cannot join channel (+l)");
        return;
    }

    members.push_back(client);
    removeFromInviteList(client); 
}

void Channel::removeMember(t_client* client) {
    members.erase(std::remove(members.begin(), members.end(), client), members.end());
    removeOperator(client);
}

bool Channel::isMember(const t_client* client) const {
    return std::find(members.begin(), members.end(), client) != members.end();
}

bool Channel::isOperator(const t_client* client) const {
    return std::find(operators.begin(), operators.end(), client) != operators.end();
}

// -------------------- Operators --------------------
void Channel::addOperator(t_client* client) {
    if (isMember(client) && !isOperator(client)) {
        operators.push_back(client);
    }
}

void Channel::removeOperator(t_client* client) {
    operators.erase(std::remove(operators.begin(), operators.end(), client), operators.end());
}

// -------------------- Modes --------------------
void Channel::setTopic(const std::string& newTopic, Client *main, t_client* sender) {
    if (topicRestricted && (!sender || !isOperator(sender))) {
        if (sender)
            main->sendServerMessage(sender->fd, ":ft_irc 482 " + getNickname(sender) + " " + name + " :You're not channel operator");
        return;
    }
    topic = newTopic;
}

void Channel::setInviteOnly(bool value) { inviteOnly = value; }
void Channel::setKey(const std::string& newKey) { key = newKey; }
void Channel::setUserLimit(size_t limit) { userLimit = limit; }

// -------------------- IRC Commands --------------------
bool Channel::kick(Client *main, t_client* sender, t_client* target) {
    if (!sender || !target) return false;
    if (!isOperator(sender)) return false;
    if (!isMember(target)) return false;

    std::string msg = ":" + sender->nick + "!" + sender->user + "@localhost " + "KICK " + name + " " + target->nick + " :kicked";

    for (std::vector<t_client*>::iterator it = members.begin(); it != members.end(); ++it)
        main->sendServerMessage((*it)->fd, msg);

    removeMember(target);
    return true;
}

bool Channel::invite(Client *main, t_client* sender, t_client* target) {
    if (!sender || !target)
        return false;

    if (!isOperator(sender))
        return false;

    if (isInvited(target) || isMember(target))
        return false;

    inviteList.push_back(target);
    main->sendServerMessage(sender->fd, ":ft_irc 341 " + getNickname(sender) + " " + getNickname(target) + " :" + name);
    return true;
}

bool Channel::mode(t_client* sender, char flag, bool value, t_client* target, const std::string& param) {
    if (!sender)
        return false;

    if (!isOperator(sender))
        return false;

    switch (flag) {
        case 'i':
            setInviteOnly(value);
            break;
        case 't':
            topicRestricted = value;
            break;
        case 'k':
            setKey(value ? param : "");
            break;
        case 'o':
            if (target) {
                if (value)
                    addOperator(target);
                else
                    removeOperator(target);
            }
            break;
        case 'l':
            setUserLimit(value ? (size_t)std::atoi(param.c_str()) : 0);
            break;
        default:
            return false;
    }
    return true;
}

// -------------------- Getters --------------------
const std::string& Channel::getName() const { return name; }
const std::string& Channel::getTopic() const { return topic; }
size_t Channel::getUserCount() const { return members.size(); }

bool Channel::isInvited(t_client* client) const {
    return std::find(inviteList.begin(), inviteList.end(), client) != inviteList.end();
}

void Channel::removeFromInviteList(t_client* client) {
    inviteList.erase(std::remove(inviteList.begin(), inviteList.end(), client), inviteList.end());
}

bool Channel::isInviteOnly() const {
    return inviteOnly;
}

bool Channel::isTopicRestricted() const {
    return topicRestricted;
}

const std::string& Channel::getKey() const {
    return key;
}

size_t Channel::getUserLimit() const {
    return userLimit;
}

void Channel::broadcast(Client *main, const std::string& msg, int excludeFd) {
    for (std::vector<t_client*>::iterator it = members.begin(); it != members.end(); ++it)
        if ((*it)->fd != excludeFd)
            main->sendServerMessage((*it)->fd, msg);
}

const std::vector<t_client*>& Channel::getMembers() const {
    return members;
}