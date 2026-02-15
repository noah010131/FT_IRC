// Channel.hpp
#pragma once
#include <string>
#include <set>

class Channel {
public:
    std::string name;
    std::string topic;
    std::string key;        // 채널 비밀번호
    size_t userLimit;       // 최대 유저 수
    std::set<int> clients;  // fd 저장
    std::set<int> operators;// 운영자 fd 저장

    Channel(const std::string &name) : name(name), userLimit(0) {}
};

