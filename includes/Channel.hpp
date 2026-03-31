#pragma once
#include <string>
#include <set>
#include <sstream>
#include <ctime>

class Channel
{
    public:
        std::string name;
        std::string key;        // secret key channel (k)
        size_t userLimit;       // maximum user on channel (l)
        bool inviteOnly;     // invite only to access channel (i)
        std::set<int> invited; // list invited members 
        bool topicRestricted; // restricted topic (t) 
        std::set<int> clients;  // list clients's fd
        std::set<int> operators;// fd of operator (o)
        std::string topic; // topic of channel
        time_t _createdAt; // time of created channel

        std::string getModes(bool isMember);
        std::string getCreationTime();


        Channel(const std::string &name) : name(name), userLimit(0), inviteOnly(false), topicRestricted(false), _createdAt(time(NULL)){}
};