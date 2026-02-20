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
        std::string msgBuffer;
        std::string nick;
        std::string user;
        std::string realname;
        std::string hostname;
        bool authed;
    	bool passOk;
        bool isTerminating; // 서버 종료 시 클라이언트에게 종료 메시지를 한 번만 보내도록 하는 플래그
        std::set<std::string> joinedChannels;

        Client();
        Client(int fd);
};
