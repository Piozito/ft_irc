#include "network/Server.hpp"

int main(int argc, char **argv)
{
	if (argc != 3)
	{
		std::cerr << "Usage: " << argv[0] << " <port> <password>" << std::endl;
		return 1;
	}

	int port = atoi(argv[1]);
	if (port <= 0 || port > 65535)
	{
		std::cerr << "Invalid port number" << std::endl;
		return 1;
	}

	try {
		Server server(port, argv[2]);
		server.serverer();
	}
	catch (const std::exception& e) {
		std::cerr << "Server error: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
