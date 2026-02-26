#include "../inc/Channel.hpp"
#include "../inc/Client.hpp"
#include <iostream>

int main() {
    // Create clients
    Client goku("Goku");
    Client aura("Aura Farmer");
    Client charlie("Charlie Kirk");

    // Create channel
    Channel general("#general");

    // Add members
    general.addMember(&goku);
    general.addMember(&aura);
    general.addMember(&charlie);

    // Make Goku operator
    general.addOperator(&goku);

    // Broadcast welcome message
    general.broadcast("Welcome to #general!", &goku);

    // Set and print topic
    general.setTopic("Aura Farming and toughness");
    std::cout << "Topic is now: " << general.getTopic() << std::endl;

    // Operator commands tests
    std::cout << "\n--- Testing KICK ---\n";
    general.kick(&goku, &charlie); // Goku kicks Charlie

    std::cout << "\n--- Testing INVITE ---\n";
    general.invite(&goku, &charlie); // Goku invites Charlie back

    std::cout << "\n--- Testing MODE ---\n";
    general.mode(&goku, 'i', true); // Set invite-only
    general.mode(&goku, 'l', true); // Set user limit
    general.mode(&goku, 'o', true, &aura); // Make Aura operator

    std::cout << "\nCurrent members:\n";
    general.broadcast("Check members after commands!", NULL);

    return 0;
}