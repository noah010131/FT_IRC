#pragma once
#include <string>
#include <cctype>
#include <set>
#include <iostream>

class Server;

class Client
{
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
