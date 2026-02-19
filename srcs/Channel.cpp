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
        params += " " + std::to_string(userLimit);
    }

    // 만약 설정된 모드가 하나도 없어서 "+"만 남았다면?
    // 취향에 따라 "+"만 보내거나, 기본 모드(t 등)를 넣어줍니다.
    return modes + params;
}

std::string Channel::getCreationTime()
{
    std::stringstream ss;
    ss << _createdAt;
    return ss.str();
}