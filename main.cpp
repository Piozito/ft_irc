#include <iostream>

int main(int ac, char **av)
{
    if(ac != 3)
    {
        std::cout << "Valid usage: ./ircserv <port> <password>" << std::endl;
        return -1;
    }

}