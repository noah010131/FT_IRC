#include "Server.hpp"
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <cerrno>
#include <sstream>

Server::Server(int port, const std::string& password)
    : _password(password) {

		/* socket 생성 */
		/* AF_INET: IPv4 주소 체계 */
		/* SOCK_STREAM: 연결 지향 서비스 */
		/* 0: 기본 프로토콜 (TCP) */
    _listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (_listenFd < 0)
        throw std::runtime_error("socket failed");

	/* SOL_SOCKET: 소켓 계층 */
	/* SO_REUSEADDR: 주소 재사용 */
    int yes = 1;
    setsockopt(_listenFd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

	/*구조체 초기화*/
    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
	/* INADDR_ANY: 모든 인터넷 주소 */
    addr.sin_addr.s_addr = INADDR_ANY;
	/* htons: 호스트 바이트 순서를 네트워크 바이트 순서로 변환 */
    addr.sin_port = htons(port);

	/* bind: 소켓을 특정 주소와 포트에 바인딩 */
    if (bind(_listenFd, (sockaddr*)&addr, sizeof(addr)) < 0)
        throw std::runtime_error("bind failed");

	/* listen: 소켓을 수신 대기 상태로 전환 */
    if (listen(_listenFd, 128) < 0)
        throw std::runtime_error("listen failed");

	/* fcntl: 파일 디스크립터 설정 */
	/* F_SETFL: 파일 디스크립터 설정 모드 변경 */
	/* O_NONBLOCK: 비블로킹 모드 */
    fcntl(_listenFd, F_SETFL, O_NONBLOCK);

	/* pollfd 구조체 초기화 */
    pollfd pfd;
    pfd.fd = _listenFd;
    pfd.events = POLLIN;
    _pfds.push_back(pfd);
}

void Server::run() {
	/* poll: 소켓 상태 확인 */
	/* &_pfds[0]: 파일 디스크립터 배열 */
	/* _pfds.size(): 파일 디스크립터 배열 크기 */
	/* -1: 블로킹 모드 */
    while (true) {
        if (poll(&_pfds[0], _pfds.size(), -1) < 0)
            throw std::runtime_error("poll failed");

        for (size_t i = 0; i < _pfds.size(); ++i) {
            if (_pfds[i].revents & POLLIN) {
                if (_pfds[i].fd == _listenFd)
                    acceptNewClient();
                else
                    handleClientData(_pfds[i].fd);
            }
        }
    }
}

void Server::acceptNewClient() {
    sockaddr_in clientAddr;
    socklen_t len = sizeof(clientAddr);
	/* accept: 클라이언트 연결 요청 수락 */
    int clientFd = accept(_listenFd, (sockaddr*)&clientAddr, &len);
	/* 클라이언트 연결 요청 수락 실패 */
    if (clientFd < 0)
        return;

    fcntl(clientFd, F_SETFL, O_NONBLOCK);

	/* pollfd 구조체 초기화 */
    pollfd pfd;
    pfd.fd = clientFd;
    pfd.events = POLLIN;
    _pfds.push_back(pfd);

	/* 클라이언트 정보 저장 */
    _clients.insert(std::make_pair(clientFd, Client(clientFd)));

    std::cout << "New client connected: " << clientFd << std::endl;
}

void Server::handleClientData(int fd)
{
    char buf[512]; // IRC 최대 길이 미만
    int bytes = recv(fd, buf, sizeof(buf) - 1, 0);

    // 연결 종료
    if (bytes == 0)
    {
		std::cout << "Client disconnected: " << fd << std::endl;
		removeClient(fd);
		return;
	}
	if (bytes < 0)
    {
		if (errno == EWOULDBLOCK || errno == EAGAIN) {
			// non-blocking, 일시적 없음 → 무시
			return;
		}
		std::cerr << "recv error on fd " << fd << std::endl;
		removeClient(fd);
		return;
	}
	

    buf[bytes] = '\0';
    Client &client = _clients[fd];

	const size_t MAX_BUFFER = 4096;

	if (client.buffer.size() + bytes > MAX_BUFFER)
	{
		std::cerr << "Buffer overflow from fd " << fd << std::endl;
		removeClient(fd);
		return;
	}
	
    client.buffer += buf; // 기존 버퍼에 누적
	
	size_t pos;

	while ((pos = client.buffer.find("\n")) != std::string::npos)
    {
        std::string line = client.buffer.substr(0, pos);
        client.buffer.erase(0, pos + 1); // 처리한 부분 제거

		// \r 제거 (IRC는 \r\n으로 끝남)
		if (!line.empty() && line[line.size() - 1] == '\r')
			line.erase(line.size() - 1);

		// 빈 라인은 무시
		if (line.empty())
			continue;

        // line = 실제 IRC 명령
        std::cout << "Received command from fd " << fd << ": " << line << std::endl;
		processCommand(client, line);
	}
}

void Server::processCommand(Client &client, const std::string &message) {
    std::istringstream iss(message);
    std::string cmd;
    iss >> cmd;

    if (cmd.empty())
        return;
    if (cmd == "PASS")
    {
        std::string pass;
        iss >> pass;
        if (pass != _password)
        {
            sendError(client, "Invalid password");
            return;
        }
        client.passOk = true;
    }
    else if (cmd == "NICK")
    {
        std::string nick;
        iss >> nick;
        if (nick.empty())
        {
            sendError(client, "Nickname is required");
            return;
        }
        client.nick = nick;
    }
    else if (cmd == "USER")
    {
        if (!client.passOk)
        {
            sendError(client, "You must send PASS first");
            return;
        }
        if (client.nick.empty())
        {
            sendError(client, "You must set NICK first");
            return;
        }
        std::string username;
        iss >> username;
        if (username.empty())
        {
            sendError(client, "Username is required");
            return;
        }

        client.user = username;
        client.authed = true;

        std::string welcome = ":ircserv 001 " + client.nick + " :Welcome!\r\n";
        int bytesSent = send(client.fd, welcome.c_str(), welcome.size(), 0);
        if (bytesSent < 0) {
            std::cerr << "Send failed for fd " << client.fd << std::endl;
            removeClient(client.fd);
        }
    }
    else if (cmd == "MODE")
    {
        std::string chanName;
        iss >> chanName;
        std::string modeStr;
        iss >> modeStr;
        handleMode(client, chanName, modeStr, iss);
    }
    else
    {
        if (!client.authed)
		{
            sendError(client, "You must register first");
			return;
        }
        else if (cmd == "JOIN")
		{
			handleJoin(client, iss);
		}
		else if (cmd == "PRIVMSG")
		{
			handlePrivmsg(client, iss);
		}
		
    }
}

void Server::sendError(Client &client, const std::string &msg)
{
    std::string error = "ERROR :" + msg + "\r\n";
    int bytesSent = send(client.fd, error.c_str(), error.size(), 0);
    if (bytesSent < 0)
	{
        std::cerr << "Send failed for fd " << client.fd << std::endl;
        removeClient(client.fd);
    }
}

void Server::removeClient(int fd)
{
    std::map<int, Client>::iterator clientIt = _clients.find(fd);
    if (clientIt != _clients.end())
    {
        Client &client = clientIt->second;

        // 지울 채널 이름 따로 수집 (iterator 무효화 방지)
        std::vector<std::string> emptyChannels;

        for (std::set<std::string>::iterator it = client.joinedChannels.begin();
             it != client.joinedChannels.end(); ++it)
        {
            std::map<std::string, Channel>::iterator chanIt = _channels.find(*it);
            if (chanIt != _channels.end())
            {
                chanIt->second.clients.erase(fd);

                if (chanIt->second.clients.empty())
                    emptyChannels.push_back(chanIt->first);
            }
        }

        // 빈 채널 삭제 (반복 끝난 후)
        for (size_t i = 0; i < emptyChannels.size(); ++i)
            _channels.erase(emptyChannels[i]);
    }

    // pollfd에서 제거
    for (size_t i = 0; i < _pfds.size(); ++i)
    {
        if (_pfds[i].fd == fd)
        {
            _pfds.erase(_pfds.begin() + i);
            break;
        }
    }

    close(fd);
    _clients.erase(fd);

    std::cout << "Client disconnected: " << fd << std::endl;
}


Channel& Server::getOrCreateChannel(const std::string &name)
{
    std::map<std::string, Channel>::iterator it = _channels.find(name);
    if (it == _channels.end())
	{
        _channels.insert(std::make_pair(name, Channel(name)));
        it = _channels.find(name);
    }
    return it->second;
}

void Server::broadcastToChannel(Channel &chan, const std::string &msg)
{
	for (std::set<int>::iterator it = chan.clients.begin();
	     it != chan.clients.end(); )
	{
		std::set<int>::iterator current = it;
		++it;

		int fd = *current;
		int bytesSent = send(fd, msg.c_str(), msg.size(), 0);
		if (bytesSent < 0)
		{
			std::cerr << "Send failed for fd " << fd << std::endl;
			removeClient(fd);
		}
	}
}

void Server::handleJoin(Client &client, std::istringstream &iss)
{
    std::string channelName;
    std::string key;
    iss >> channelName >> key;
    if (channelName.empty()) return;

    Channel &chan = getOrCreateChannel(channelName);

    // invite-only 체크
    if (chan.inviteOnly && chan.invited.find(client.fd) == chan.invited.end())
    {
        sendError(client, "Invite only channel");
        return;
    }
    
    if (!chan.key.empty() && chan.key != key)
    {
        if (key.empty())
            sendError(client, "this channel needs key");
        else
            sendError(client, "Bad Channel key");
        return;
    }
    // 유저 제한 체크 (+l)
    if (chan.userLimit > 0 && (size_t)chan.clients.size() >= chan.userLimit)
    {
        sendError(client, "Channel is full");
        return;
    }
    chan.clients.insert(client.fd);
    client.joinedChannels.insert(channelName);

    if (chan.clients.size() == 1)
        chan.operators.insert(client.fd);
        
    std::string joinMsg = ":" + client.nick + " JOIN " + channelName + "\r\n";
    broadcastToChannel(chan, joinMsg);
}

Client* Server::findClientByNick(const std::string &nick)
{
    for (std::map<int, Client>::iterator it = _clients.begin();
         it != _clients.end();
         ++it)
    {
        if (it->second.nick == nick)
            return &it->second;
    }
    return NULL;
}

void Server::sendToChannel(const std::string &channel, const std::string &msg, int exceptFd)
{
    std::map<std::string, Channel>::iterator it = _channels.find(channel);
    if (it == _channels.end())
        return;

    Channel &chan = it->second;

    for (std::set<int>::iterator fdIt = chan.clients.begin();
         fdIt != chan.clients.end(); )
    {
        std::set<int>::iterator current = fdIt;
        ++fdIt;

        int fd = *current;
        if (fd != exceptFd)
        {
            int bytesSent = send(fd, msg.c_str(), msg.size(), 0);
            if (bytesSent < 0)
            {
                std::cerr << "Send failed for fd " << fd << std::endl;
                removeClient(fd);
            }
        }
    }
}


void Server::handlePrivmsg(Client &client, std::istringstream &iss)
{
    if (!client.authed)
    {
        sendError(client, "You must register first");
        return;
    }

    std::string target;
    iss >> target;

    if (target.empty())
    {
        sendError(client, "No recipient given");
        return;
    }

    std::string message;
    std::getline(iss, message);
	

    if (message.size() > 0 && message[0] == ' ')
        message.erase(0, 1);
	if (message.size() > 0 && message[0] == ':')
        message.erase(0, 1);

    if (message.empty())
    {
        sendError(client, "No text to send");
        return;
    }

	// 채널 우선
	std::map<std::string, Channel>::iterator chanIt = _channels.find(target);
    if (chanIt != _channels.end())
    {
        std::string full = ":" + client.nick + " PRIVMSG " + target + " :" + message + "\r\n";
        sendToChannel(target, full, client.fd);
        return;
    }

    // 닉네임
    std::map<int, Client>::iterator it;
    for (it = _clients.begin(); it != _clients.end(); ++it)
    {
        if (it->second.nick == target)
        {
            std::string full = ":" + client.nick + " PRIVMSG " + target + " :" + message + "\r\n";
            send(it->second.fd, full.c_str(), full.size(), 0);
            return;
        }
    }

    sendError(client, "No such nick/channel");
}

void Server::handleMode(Client &client, const std::string &chanName, const std::string &modeStr, std::istringstream &iss)
{
    std::map<std::string, Channel>::iterator it = _channels.find(chanName);
    if (it == _channels.end())
    {
        sendError(client, "No such channel");
        return;
    }
    Channel &chan = it->second;
// operator 체크
    if (chan.operators.find(client.fd) == chan.operators.end())
    {
        sendError(client, "You're not channel operator");
        return;
    }

    char action = modeStr[0]; // + 또는 -
    for (size_t i = 1; i < modeStr.size(); ++i)
    {
        char m = modeStr[i];
        if (m == '+' || m == '-')
        {
            action = m;
            continue;
        }

        if (!action)
        {
            sendError(client, "Mode action (+/-) missing");
            return ;
        }
        switch (m)
        {
            case 'i':
                chan.inviteOnly = (action == '+');
                break;
            case 't':
                chan.topicRestricted = (action == '+');
                break;
            case 'k':
            {
                std::string key;
                iss >> key;
                if (action == '+')
                {
                    if (key.empty())
                    {
                        sendError(client, "Need key for +k");
                        return;
                    }
                    if (key.length() > 32)
                    {
                        sendError(client, "key too long (>32)");
                        return;
                    }
                    chan.key = key;
                }
                else if (action == '-')
                    chan.key.clear();
                break;
            }
            case 'o':
            {
                std::string nick;
                iss >> nick;
                if (nick.empty())
                {
                    sendError(client, "Need nickname for +o/-o");
                    return;
                }
                int fd = getFdByNick(nick);
                if (fd == -1)
                {
                    sendError(client, "No such nick");
                    return;
                }
                if (action == '+')
                    chan.operators.insert(fd);
                else if (action == '-')
                    chan.operators.erase(fd);
                break;
            }
            case 'l':
            {
            int limit = 0;
            if (action == '+')
            {
                iss >> limit;
                if (!limit)
                {
                    sendError(client, "Need limit for +l");
                    return;
                }
                if (limit <= 0)
                {
                    sendError(client, "Invalid user limit");
                    return;
                }
                if (limit > 99999)
                {
                    sendError(client, "user limit too big (>99999)");
                    return;
                }
                chan.userLimit = limit;
            }
            else if (action == '-')
                chan.userLimit = 0;
            break;
            }
            default :
                sendError(client, "Unknown mode");
                return;
        }
    }
    // 변경 완료 메시지 브로드캐스트
    std::string msg = ":" + client.nick + " MODE " + chanName + " " + modeStr + "\r\n";
    broadcastToChannel(chan, msg);
}

int Server::getFdByNick(const std::string &nick)
{
    std::map<int, Client>::iterator it = _clients.begin();
    for (; it != _clients.end(); ++it)
    {
        if (it->second.nick == nick)
            return it->first; // fd 반환
    }
    return -1; // 못 찾음
}
