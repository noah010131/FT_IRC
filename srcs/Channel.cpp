#include "Channel.hpp"

Channel::Channel()
    : inviteOnly(false), topicOpOnly(false), userLimit(-1) {}

Channel::Channel(const std::string& name)
    : name(name), inviteOnly(false), topicOpOnly(false), userLimit(-1) {}
