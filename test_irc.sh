#!/bin/bash

# 1. 배경 유저 30명 접속 (기존 로직)
echo "--- 30명의 유저 접속 시작 ---"
for i in {1..5}; do
    (
        echo "PASS 0131"
        echo "NICK user$i"
        echo "USER u$i 0 * :real"
        echo "JOIN #channel"
        sleep 1
        echo "PRIVMSG #channel :Hello, I am user$i"
    ) | nc 127.0.0.1 6667 &
done

sleep 2 # 30명이 입장할 시간을 줍니다.
echo "--- 연속 명령어(printf) 테스트 시작 ---"

# 2. 문제의 연속 명령어 전송 테스트
# \r\n을 명시적으로 포함하여 서버가 이를 잘 쪼개는지 확인합니다.
# 수정된 printf 테스트 부분
(printf "PASS 0131\r\nNICK tester\r\nUSER t 0 * :tester\r\nJOIN #channel\r\nPRIVMSG #channel :I am testing multiple commands!\r\n"; sleep 2) | nc 127.0.0.1 6667

echo "--- 테스트 완료 ---"
