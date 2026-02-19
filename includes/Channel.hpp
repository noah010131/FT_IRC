#pragma once
#include <string>
#include <set>
#include <sstream>
#include <ctime>

class Channel
{
    public:
        std::string name;
        std::string key;        // 채널 비밀번호 (k)
        size_t userLimit;       // 최대 유저 수 (l)
        bool inviteOnly;     // 초대 전용 모드 (i)
        std::set<int> invited; // 초대받은 사용자 
        bool topicRestricted; // topic 권한 제한 모드 (t) 
        std::set<int> clients;  // fd 저장
        std::set<int> operators;// 운영자 fd 저장 및 권한 (o)
        std::string topic;
        time_t _createdAt; // 채널 생성 시간

        std::string getModes();
        std::string getCreationTime();


        Channel(const std::string &name) : name(name), userLimit(0), inviteOnly(false), topicRestricted(false), _createdAt(time(NULL)){}
};