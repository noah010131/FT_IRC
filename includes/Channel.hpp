#pragma once
#include <string>
#include <set>

class Channel {
public:
    std::string name;
    std::string topic;
    std::set<int> users;
    std::set<int> operators;

    bool inviteOnly;
    bool topicOpOnly;
    std::string key;
    int userLimit;

    Channel();
    Channel(const std::string& name);
};
