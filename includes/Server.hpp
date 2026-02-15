#pragma once
#include <map>
#include <vector>
#include <string>
#include <poll.h>

#include "Client.hpp"
#include "Channel.hpp"

class Server {
private:
    int _listenFd;
    std::vector<struct pollfd> _pfds;
    std::map<int, Client> _clients;
    std::map<std::string, Channel> _channels;
    std::string _password;

public:
    Server(int port, const std::string& password);
    void run();

private:
    void acceptNewClient();
    void handleClientData(int fd);
	void processCommand (Client &client, const std::string &message);
    void removeClient(int fd);
};
