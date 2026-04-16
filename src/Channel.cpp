#include "../inc/Channel.hpp"
#include <iostream>
#include <cstdlib>

Channel::Channel(const std::string& name)
    : name(name), topic(""), inviteOnly(false), topicRestricted(false), key(""), userLimit(0) {}

// -------------------- Members --------------------
void Channel::addMember(Client* client, const std::string& providedKey) {
    if (isMember(client))
        return;

    if (inviteOnly && !isInvited(client)) {
        client->sendMessage("473 " + client->getNickname() + " " + name + " :Cannot join channel (+i)");
        return;
    }

    if (!key.empty() && providedKey != key) {
        client->sendMessage("475 " + client->getNickname() + " " + name + " :Cannot join channel (+k)");
        return;
    }

    if (userLimit > 0 && members.size() >= userLimit) {
        client->sendMessage("471 " + client->getNickname() + " " + name + " :Cannot join channel (+l)");
        return;
    }

    members.push_back(client);
    removeFromInviteList(client);
}

void Channel::removeMember(Client* client) {
    members.erase(std::remove(members.begin(), members.end(), client), members.end());
    removeOperator(client);
}

bool Channel::isMember(Client* client) const {
    return std::find(members.begin(), members.end(), client) != members.end();
}

bool Channel::isOperator(Client* client) const {
    return std::find(operators.begin(), operators.end(), client) != operators.end();
}

// -------------------- Operators --------------------
void Channel::addOperator(Client* client) {
    if (isMember(client) && !isOperator(client)) {
        operators.push_back(client);
    }
}

void Channel::removeOperator(Client* client) {
    operators.erase(std::remove(operators.begin(), operators.end(), client), operators.end());
}

// -------------------- Modes --------------------
void Channel::setTopic(const std::string& newTopic, Client* sender) {
    if (topicRestricted && (!sender || !isOperator(sender))) {
        if (sender)
            sender->sendMessage("482 " + sender->getNickname() + " " + name + " :You're not channel operator");
        return;
    }
    topic = newTopic;
}

void Channel::setInviteOnly(bool value) { inviteOnly = value; }
void Channel::setKey(const std::string& newKey) { key = newKey; }
void Channel::setUserLimit(size_t limit) { userLimit = limit; }

// -------------------- IRC Commands --------------------
bool Channel::kick(Client* sender, Client* target) {
    if (!sender || !target)
        return false;

    if (!isOperator(sender))
        return false;

    if (isMember(target)) {
        removeMember(target);
        broadcast(target->getNickname() + " has been kicked by " + sender->getNickname(), sender);
        return true;
    }
    return false;
}

bool Channel::invite(Client* sender, Client* target) {
    if (!sender || !target)
        return false;

    if (!isOperator(sender))
        return false;

    if (isInvited(target) || isMember(target))
        return false;

    inviteList.push_back(target);
    target->sendMessage("341 " + sender->getNickname() + " " + target->getNickname() + " " + name);
    return true;
}

bool Channel::mode(Client* sender, char flag, bool value, Client* target, const std::string& param) {
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

// -------------------- Broadcast --------------------
void Channel::broadcast(const std::string& message, Client* sender) {
    for (size_t i = 0; i < members.size(); ++i) {
        if (members[i] != sender) {
            members[i]->sendMessage(message);
        }
    }
}

// -------------------- Getters --------------------
const std::string& Channel::getName() const { return name; }
const std::string& Channel::getTopic() const { return topic; }
size_t Channel::getUserCount() const { return members.size(); }

bool Channel::isInvited(Client* client) const {
    return std::find(inviteList.begin(), inviteList.end(), client) != inviteList.end();
}

void Channel::removeFromInviteList(Client* client) {
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