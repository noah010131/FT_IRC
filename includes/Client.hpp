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
        std::string msgBuffer; // all of message have to send in poll()
        std::string nick;
        std::string user;
        std::string realname;
        std::string hostname;
        bool authed;
    	bool passOk;
        bool isTerminating;
        std::set<std::string> joinedChannels;

        Client();
        Client(int fd);
};
