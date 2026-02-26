#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <iostream>

class Client {
private:
    std::string nickname;

public:
    Client(const std::string& nick) : nickname(nick) {}

    const std::string& getNickname() const {
        return nickname;
    }

    // Mock sendMessage for testing Channel broadcasting
    void sendMessage(const std::string& message) {
        std::cout << nickname << " receives: " << message << std::endl;
    }
};

#endif