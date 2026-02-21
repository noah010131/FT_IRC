#include "Channel.hpp"

std::string Channel::getModes() {
    std::string modes = "+";
    std::string params = "";

    if (inviteOnly) modes += "i";
    if (topicRestricted) modes += "t";
    if (!key.empty()) {
        modes += "k";
        params += " " + key;
    }
    if (userLimit > 0) {
        modes += "l";
        std::stringstream ss;
		ss << userLimit;
		params += " " + ss.str();
    }
    return modes + params;
}

std::string Channel::getCreationTime()
{
    std::stringstream ss;
    ss << _createdAt;
    return ss.str();
}