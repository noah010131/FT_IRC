#pragma once
#include <string>

class Client {
public:
    int fd;
    std::string buffer;
    std::string nick;
    std::string user;
    bool authed;

    Client();
    Client(int fd);
};
