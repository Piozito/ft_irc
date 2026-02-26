#include "../inc/Channel.hpp"
#include <iostream>

Channel::Channel(const std::string& name)
    : name(name), topic(""), inviteOnly(false), key(""), userLimit(0) {}

// -------------------- Members --------------------
void Channel::addMember(Client* client) {
    if (!isMember(client)) {
        if (userLimit == 0 || members.size() < userLimit) {
            members.push_back(client);
        } else {
            std::cout << client->getNickname() << " cannot join, user limit reached.\n";
        }
    }
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
void Channel::setTopic(const std::string& newTopic) { topic = newTopic; }
void Channel::setInviteOnly(bool value) { inviteOnly = value; }
void Channel::setKey(const std::string& newKey) { key = newKey; }
void Channel::setUserLimit(size_t limit) { userLimit = limit; }

// -------------------- IRC Commands --------------------
bool Channel::kick(Client* sender, Client* target) {
    if (!isOperator(sender)) return false;
    if (isMember(target)) {
        removeMember(target);
        broadcast(target->getNickname() + " has been kicked by " + sender->getNickname(), sender);
        return true;
    }
    return false;
}

bool Channel::invite(Client* sender, Client* target) {
    if (!isOperator(sender)) return false;
    // For simplicity, invitation just prints a message here
    std::cout << target->getNickname() << " has been invited to " << name << " by " << sender->getNickname() << "\n";
    return true;
}

bool Channel::mode(Client* sender, char flag, bool value, Client* target) {
    if (!isOperator(sender)) return false;

    switch (flag) {
        case 'i': setInviteOnly(value); break;
        case 't': /* restrict TOPIC to operators */ break; // logic to check in Client/Server later
        case 'k': setKey(value ? "set_key" : ""); break; // placeholder, actual key handled elsewhere
        case 'o': if (target) { value ? addOperator(target) : removeOperator(target); } break;
        case 'l': setUserLimit(value ? 10 : 0); break; // example: set limit to 10 if true, 0 = no limit
        default: return false;
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