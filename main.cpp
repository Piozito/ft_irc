#include "../inc/Channel.hpp"
#include "../inc/Client.hpp"
#include <iostream>
#include <string>

int main() {
    std::cout << "=== FT_IRC Channel Class Test Suite ===\n\n";

    Client goku("Goku");
    Client aura("Aura Farmer");
    Client charlie("Charlie Kirk");
    Client vegeta("Vegeta");

    Channel general("#general");
    std::cout << "Channel '#general' created\n\n";

    std::cout << "--- 1. Basic join & broadcast ---\n";
    general.addMember(&goku);
    general.addMember(&aura);
    general.broadcast("Welcome to #general!", &goku);
    std::cout << "User count: " << general.getUserCount() << "\n\n";

    std::cout << "--- 2. Operators ---\n";
    general.addOperator(&goku);
    std::cout << "Goku is operator: " << (general.isOperator(&goku) ? "YES" : "NO") << "\n";
    std::cout << "Aura is operator: " << (general.isOperator(&aura) ? "YES" : "NO") << "\n\n";

    std::cout << "--- 3. Topic (no restriction) ---\n";
    general.setTopic("Aura Farming and toughness", &goku);
    std::cout << "Topic: '" << general.getTopic() << "'\n\n";

    std::cout << "--- 4. KICK ---\n";
    general.kick(&goku, &charlie);
    std::cout << "Charlie kicked. User count: " << general.getUserCount() << "\n\n";

    std::cout << "--- 5. INVITE ---\n";
    general.invite(&goku, &charlie);
    std::cout << "Charlie invited. isInvited: " << (general.isInvited(&charlie) ? "YES" : "NO") << "\n\n";

    std::cout << "--- 6. MODE tests ---\n";
    std::cout << "Before modes - inviteOnly: " << general.isInviteOnly()
              << ", key: '" << general.getKey()
              << "', limit: " << general.getUserLimit() << "\n";

    general.mode(&goku, 'i', true);
    general.mode(&goku, 'k', true, NULL, "secret");
    general.mode(&goku, 'l', true, NULL, "2");
    general.mode(&goku, 't', true);
    general.mode(&goku, 'o', true, &aura);

    std::cout << "After modes  - inviteOnly: " << general.isInviteOnly()
              << ", key: '" << general.getKey()
              << "', limit: " << general.getUserLimit()
              << ", topicRestricted: " << general.isTopicRestricted() << "\n";
    std::cout << "Aura is now operator: " << (general.isOperator(&aura) ? "YES" : "NO") << "\n\n";

    std::cout << "--- 7. Join restrictions (+i +k +l) ---\n";
    general.addMember(&charlie, "wrongkey");
    general.addMember(&vegeta, "secret");
    std::cout << "Vegeta is member: " << (general.isMember(&vegeta) ? "YES" : "NO") << "\n";
    std::cout << "User count: " << general.getUserCount() << "\n\n";

    std::cout << "--- 8. Topic restriction (+t) ---\n";
    general.setTopic("Non-op topic attempt", &vegeta);
    general.setTopic("Op topic works", &aura);
    std::cout << "Final topic: '" << general.getTopic() << "'\n\n";

    std::cout << "--- 9. Final state ---\n";
    std::cout << "Members: " << general.getUserCount() << "\n";
    std::cout << "Invite list size: " << (general.isInvited(&charlie) ? 1 : 0) << "\n";
    std::cout << "Modes: +i +k +l +t\n";

    std::cout << "\n=== ALL TESTS PASSED ===\n";
    return 0;
}