#pragma once
#include <string>
#include <set>

class Client {
public:
    int fd;
    std::string buffer;
    std::string nick;
    std::string user;
    bool authed;
	bool passOk;
    std::set<std::string> joinedChannels;

    Client();
    Client(int fd);
};
