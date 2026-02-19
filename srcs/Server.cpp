#include "Server.hpp"
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <cerrno>
#include <sstream>
#include <cstdlib>

Server* Server::_instance = NULL;

void handleSignal(int sig)
{
    (void)sig;
}

// 안전 종료
void Server::shutdown()
{
    std::cout << "\nShutting down server...\n";

    for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it)
    {
        int fd = it->first;
        std::string quitMsg = ":ircserv ERROR :Server shutting down\r\n";
        send(fd, quitMsg.c_str(), quitMsg.size(), 0);
        close(fd);
    }
	if (_listenFd != -1)
	{
        close(_listenFd);
        _listenFd = -1;
    }
    _clients.clear();
    _channels.clear();
    _pfds.clear();
    std::cout << "Server terminated cleanly.\n";
}

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
	memset(&pfd, 0, sizeof(pfd));
    pfd.fd = _listenFd;
    pfd.events = POLLIN;
    _pfds.push_back(pfd);
}

void Server::run() {

    _instance = this;
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, handleSignal);
    signal(SIGTERM, handleSignal);
	/* poll: 소켓 상태 확인 */
	/* &_pfds[0]: 파일 디스크립터 배열 */
	/* _pfds.size(): 파일 디스크립터 배열 크기 */
	/* -1: 블로킹 모드 */
    while (true)
	{
		int poll_res = poll(&_pfds[0], _pfds.size(), -1);
		if (poll_res < 0 && errno == EINTR)
            break;
        if (poll_res < 0)
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
	this->shutdown();
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
	memset(&pfd, 0, sizeof(pfd));
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
	

    //buf[bytes] = '\0';
    Client &client = _clients[fd];

	const size_t MAX_BUFFER = 4096;

	if (client.buffer.size() + bytes > MAX_BUFFER)
	{
		std::cerr << "Buffer overflow from fd " << fd << std::endl;
		removeClient(fd);
		return;
	}
	
    //client.buffer += buf; // 기존 버퍼에 누적
    client.buffer.append(buf, bytes);
	
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
        if (client.passOk)
        {
            sendMsg(client, ERR_ALREADYREGISTRED, ":You may not register");
            return;
        }
        std::string pass;
        iss >> pass;
        if (pass.empty())
        {
            sendMsg(client, ERR_NEEDMOREPARAMS, "PASS :Not enough parameters");
            return;
        }
        if (pass != _password)
        {
            sendMsg(client, ERR_PASSWDMISMATCH, ":Password incorrect");
            return;
        }
        client.passOk = true;
    }
    else if (cmd == "NICK")
    {
        std::string nick;
        iss >> nick;
        if (!isValidNick(nick, client))
            return;
        client.nick = nick;
    }
    else if (cmd == "USER")
    {
        std::string username;
        iss >> username;
        if (!isValidUser(username, client))
            return ;
        client.user = username;
        client.authed = true;

        sendMsg(client, RPL_WELCOME, ":Welcome!");
    }
    else
    {
        if (!client.authed)
		{
            sendMsg(client, ERR_NOTREGISTERED, ":You have not registered");
			return;
        }
        else if (cmd == "JOIN")
			handleJoin(client, iss);
		else if (cmd == "PRIVMSG")
			handlePrivmsg(client, iss);
        else if (cmd == "MODE")
        {
            std::string chanName;
            iss >> chanName;
            std::string modeStr;
            iss >> modeStr;
            handleMode(client, chanName, modeStr, iss);
        }
        else if (cmd == "KICK")
            handleKick(client, iss);
        else if (cmd == "INVITE")
            handleInvite(client, iss);
        else if (cmd == "TOPIC")
            handleTopic(client, iss);
        else
            sendMsg(client, ERR_UNKNOWNCOMMAND, cmd + " :Unknown command");
    }
}

bool Server::nickExists(const std::string &nick) const
{
    for (std::map<int, Client>::const_iterator it = _clients.begin();
         it != _clients.end(); ++it)
    {
        if (it->second.nick == nick)
            return true;
    }
    return false;
}


bool Server::isValidNick(const std::string& nick, Client &client)
{
    if (!client.passOk)
    {
        sendMsg(client, ERR_NOTREGISTERED, ":You have not registered");
        return(false);
    }
    if (nick.empty())
    {
        sendMsg(client, ERR_NONICKNAMEGIVEN, ":No nickname given");
        return (false);
    }
    if (nickExists(nick))
    {
            sendMsg(client, ERR_NICKNAMEINUSE, nick + " :Nickname is already in use");
            return (false);
    }
    if(nick.length() > 9)
    {
        sendMsg(client, ERR_ERRONEUSNICKNAME, nick + " :Erroneus nickname (max 9 char)");
        return (false);
    }
    if (!std::isalpha(nick[0]) && std::string("[]\\`^{}|").find(nick[0]) == std::string::npos)
    {
        sendMsg(client, ERR_ERRONEUSNICKNAME, nick + " :Erroneus nickname");
        return (false);
    }
    for (size_t i = 1; i < nick.length(); ++i)
	{
        char c = nick[i];
        
        if (!std::isalnum(c) && std::string("[]\\`^{}|-").find(c) == std::string::npos)
        {
            sendMsg(client, ERR_ERRONEUSNICKNAME, nick + " :Erroneus nickname");
            return (false);
        }
    }
    return true;
}

bool Server::isValidUser(const std::string& user, Client &client)
{
    if (!client.passOk)
    {
        sendMsg(client, ERR_NOTREGISTERED, ":You have not registered");
        return(false);
    }
    if (client.nick.empty())
    {
        sendMsg(client, ERR_NOTREGISTERED, ":You must set NICK first");
        return(false);
    }
    if (user.empty())
	{
    	sendMsg(client, ERR_NEEDMOREPARAMS, "USER :Not enough parameters");
		return (false);
	}
    if (user.length() > 10)
    {
        sendMsg(client, ERR_ERRONEUSNICKNAME, user + " :Erroneus username (max 10 char)");
        return false;
    }
    for (size_t i = 0; i < user.length(); ++i)
	{
        char c = user[i];
        if (isspace(c) || c == '@' || c == ':')
        {
            sendMsg(client, ERR_ERRONEUSNICKNAME, user + " :Erroneus username");
            return false;
        }
    }
    if (client.authed)
    {
        sendMsg(client, ERR_ALREADYREGISTRED, ":You may not reregister");
        return (false);
    }
    return (true);
}

void Server::sendMsg(Client &client, const std::string &code, const std::string &msg)
{
    std::string nick = client.nick.empty() ? "*" : client.nick;
    std::string err = ":ircserv " + code + " " + nick + " " + msg + "\r\n";
    if (err.size() > 510)
        err = err.substr(0, 510);
    if (send(client.fd, err.c_str(), err.size(), 0) < 0)
        removeClient(client.fd);
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
			// removeClient(fd);
		}
	}
}

void Server::handleJoin(Client &client, std::istringstream &iss)
{
    std::string channelName;
    std::string key;
    iss >> channelName >> key;
    if (channelName.empty() || channelName[0] != '#')
    {
        sendMsg(client, ERR_NOSUCHCHANNEL, channelName + " :No such channel");
        return;
    }

    Channel &chan = getOrCreateChannel(channelName);

    // 이미 채널에 가입되어 있는지 체크
    if (chan.clients.find(client.fd) != chan.clients.end())
    {
        sendMsg(client, ERR_USERONCHANNEL, channelName + " :is already on channel");
        return;
    }
    // invite-only 체크
    if (chan.inviteOnly && chan.invited.find(client.fd) == chan.invited.end())
    {
        sendMsg(client, ERR_INVITEONLYCHAN, channelName + " :Cannot join channel (+i)");
        return;
    }
    
    if (!chan.key.empty() && chan.key != key)
    {
        sendMsg(client, ERR_BADCHANNELKEY, channelName + " :Cannot join channel (+k)");
        return;
    }
    // 유저 제한 체크 (+l)
    if (chan.userLimit > 0 && (size_t)chan.clients.size() >= chan.userLimit)
    {
        sendMsg(client, ERR_CHANNELISFULL, channelName + " :Cannot join channel (+l)");
        return;
    }
    chan.clients.insert(client.fd);
    client.joinedChannels.insert(channelName);

    if (chan.clients.size() == 1)
        chan.operators.insert(client.fd);
        
    std::string joinMsg = ":" + client.nick + " JOIN " + channelName + "\r\n";
    broadcastToChannel(chan, joinMsg);

    if (!chan.topic.empty())
        sendMsg(client, "332", channelName + " :" + chan.topic);

    std::string names = "";
    for (std::set<int>::iterator it = chan.clients.begin(); it != chan.clients.end(); ++it) {
        if (chan.operators.find(*it) != chan.operators.end())
            names += "@";
        names += getNickByFd(*it) + " ";
    }
    sendMsg(client, "353", "= " + channelName + " :" + names);
    sendMsg(client, "366", channelName + " :End of /NAMES list");
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
                // removeClient(fd);
            }
        }
    }
}


void Server::handlePrivmsg(Client &client, std::istringstream &iss)
{
    std::string target;
    iss >> target;

    std::string message;
    std::getline(iss, message);
	
    size_t first_not_space = message.find_first_not_of(" ");
    if (first_not_space != std::string::npos)
        message.erase(0, first_not_space);

    if (!message.empty() && message[0] == ':')
        message.erase(0, 1);

    if (target.empty() || message.empty())
    {
        sendMsg(client, ERR_NEEDMOREPARAMS, "PRIVMSG :Not enough parameters");
        return;
    }

	// 채널 우선
	std::map<std::string, Channel>::iterator chanIt = _channels.find(target);
    if (chanIt != _channels.end())
    {
        Channel &chan = chanIt->second;

        if (chan.clients.find(client.fd) == chan.clients.end())
        {
            sendMsg(client, ERR_NOTONCHANNEL, target + " :You're not on that channel");
            return;
        }
        std::string full = ":" + client.nick + " PRIVMSG " + target + " :" + message + "\r\n";
        sendToChannel(target, full, client.fd);
        return;
    }
    else if (target[0] == '#')
    {
        sendMsg(client, ERR_NOSUCHCHANNEL, target + " :No such channel");
        return ;
    }

    // 닉네임
    int targetFd = getFdByNick(target);
    if (targetFd != -1)
    {
        std::string full = ":" + client.nick + " PRIVMSG " + target + " :" + message + "\r\n";
        send(targetFd, full.c_str(), full.size(), 0);
        return;
    }
    sendMsg(client, ERR_NOSUCHNICK, target + " :No such nick/channel");
    return;
}

void Server::handleMode(Client &client, const std::string &chanName, const std::string &modeStr, std::istringstream &iss)
{
    std::map<std::string, Channel>::iterator it = _channels.find(chanName);
    if (it == _channels.end())
    {
        sendMsg(client, ERR_NOSUCHCHANNEL, chanName + " :No such channel");
        return;
    }
    Channel &chan = it->second;
// operator 체크
    if (chan.operators.find(client.fd) == chan.operators.end())
    {
        sendMsg(client, ERR_CHANOPRIVSNEEDED, chanName + " :You're not channel operator");
        return;
    }

    std::string appliedMode = ""; // 실제로 적용된 부호와 문자들 (+it-k 등)
    std::string appliedParams = ""; // 적용된 모드에 필요한 인자들 (pass 10 등)
    char action = '+'; // 기본값 설정

    for (size_t i = 0; i < modeStr.size(); ++i)
    {
        char m = modeStr[i];
        if (m == '+' || m == '-') {
            action = m;
            continue;
        }

        switch (m)
        {
            case 'i':
            {
                bool newValue = (action == '+');

                if (chan.inviteOnly == newValue)
                    continue;
                chan.inviteOnly = (action == '+');
                appliedMode += action;
                appliedMode += m;
                break;
            }
            case 't':
            {
                bool newValue = (action == '+');

                if (chan.topicRestricted == newValue)
                    continue;
                chan.topicRestricted = (action == '+');
                appliedMode += action;
                appliedMode += m;
                break;
            }
            case 'k':
            {
                std::string key;
                if (action == '+')
                {
                    if (!(iss >> key)) { // 인자 부족 체크
                        sendMsg(client, ERR_NEEDMOREPARAMS, "MODE +k :Not enough parameters");
                        continue; 
                    }
                    if (key.length() > 32)
                    {
                        sendMsg(client, ERR_INVALIDMODEPARAMS, chanName + " k " + key + " :Password is too long (max 32)");
                        continue; // 이 모드는 건너뛰고 다음 문자로!
                    }

                    if (chan.key == key)
                        continue;
                    chan.key = key;
                    appliedMode += "+k"; appliedParams += " " + key;
                } else {
                    chan.key.clear();
                    appliedMode += "-k";
                }
                break;
            }
            case 'o':
            {
                std::string nick;
                if (!(iss >> nick)) {
                    sendMsg(client, ERR_NEEDMOREPARAMS, "MODE +o :Not enough parameters");
                    continue;
                }
                int fd = getFdByNick(nick);
                if (fd == -1) {
                    sendMsg(client, ERR_NOSUCHNICK, nick + " :No such nick");
                    continue;
                }
                if (action == '+')
                {
                    if (chan.operators.find(fd) != chan.operators.end())
                        continue;
                    chan.operators.insert(fd);
                }
                else
                {
                    if (chan.operators.find(fd) == chan.operators.end())
                        continue;
                    chan.operators.erase(fd);
                }
                appliedMode += action; appliedMode += "o"; appliedParams += " " + nick;
                break;
            }
            case 'l':
            {
                if (action == '+')
                {
                    std::string limitStr;
                    if (!(iss >> limitStr))
                    {
                        sendMsg(client, ERR_NEEDMOREPARAMS, "MODE +l :Not enough parameters");
                        continue;
                    }
                    int limit = std::atoi(limitStr.c_str());
                    if (limit <= 0 || limit > 10000)
                    {
                        sendMsg(client, ERR_INVALIDMODEPARAMS, chanName + " l " + limitStr + " :Invalid limit (must be 1-10000)");
                        continue;
                    }
                    if ((int)chan.userLimit == limit)
                        continue;

                    chan.userLimit = limit;
                    appliedMode += "+l"; appliedParams += " " + limitStr;
                }
                else
                {
                    if (chan.userLimit == 0)
                            continue;
                    chan.userLimit = 0;
                    appliedMode += "-l";
                }
                break;
            }
            default:
                // 알 수 없는 모드는 에러 메시지 후 계속 진행
                sendMsg(client, ERR_UNKNOWNMODE, std::string(1, m) + " :is unknown mode char to me");
                break;
        }
    }

    if (!appliedMode.empty()) {
        std::string finalMsg = ":" + client.nick + " MODE " + chanName + " " + appliedMode + appliedParams + "\r\n";
        broadcastToChannel(chan, finalMsg);
    }
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

std::string Server::getNickByFd(int fd)
{
    std::map<int, Client>::iterator it = _clients.find(fd);
    if (it != _clients.end())
    {
        // 닉네임이 설정 전이라면 "*" 혹은 빈 문자열을 반환하도록 처리
        return it->second.nick.empty() ? "*" : it->second.nick;
    }
    return "Unknown"; // 만약의 상황을 대비한 예외 처리
}

void Server::handleKick(Client &client, std::istringstream &iss)
{
    std::string chanName, nick;
    iss >> chanName >> nick;

    if (chanName.empty() || nick.empty())
    {
        sendMsg(client, ERR_NEEDMOREPARAMS, "KICK :Not enough parameters");
        return;
    }

    std::map<std::string, Channel>::iterator it = _channels.find(chanName);
    if (it == _channels.end())
    {
        sendMsg(client, ERR_NOSUCHCHANNEL, chanName + " :No such channel");
        return;
    }

    Channel &chan = it->second;

    if (chan.operators.find(client.fd) == chan.operators.end())
    {
        sendMsg(client, ERR_CHANOPRIVSNEEDED, chanName + " :You're not channel operator");
        return;
    }

    int fd = getFdByNick(nick);
    if (fd == -1 || chan.clients.find(fd) == chan.clients.end())
    {
        sendMsg(client, ERR_USERNOTINCHANNEL, nick + " " + chanName + " :They aren't on that channel");
        return;
    }

    // KICK 채널 전체 + kick 당한 유저
    std::string msg = ":" + client.nick + " KICK " + chanName + " " + nick + "\r\n";
    broadcastToChannel(chan, msg);
    // send(fd, msg.c_str(), msg.size(), 0);

    // 채널에서만 제거
    chan.clients.erase(fd);
    chan.operators.erase(fd);
    chan.invited.erase(fd);
    _clients[fd].joinedChannels.erase(chanName);

    // 채널 비었으면 삭제
    if (chan.clients.empty())
        _channels.erase(chanName);
}


void Server::handleInvite(Client &client, std::istringstream &iss)
{
    std::string nick, chanName;
    iss >> nick >> chanName;
    if (nick.empty() || chanName.empty()) {
        sendMsg(client, ERR_NEEDMOREPARAMS, "INVITE :NOT enough parameters");
        return;
    }

    std::map<std::string, Channel>::iterator it = _channels.find(chanName);
    if (it == _channels.end()) {
        sendMsg(client, ERR_NOSUCHCHANNEL, chanName + " :No such channel");
        return;
    }
    Channel &chan = it->second;

    // operator 체크
    if (chan.operators.find(client.fd) == chan.operators.end()) {
        sendMsg(client, ERR_CHANOPRIVSNEEDED, chanName + " :You're not channel operator");
        return;
    }

    int fd = getFdByNick(nick);
    if (fd == -1) {
        sendMsg(client, ERR_NOSUCHNICK, nick + " :No such nick");
        return;
    }

    // 초대할 대상이 이미 채널에 있는지 확인
    if (chan.clients.find(fd) != chan.clients.end()) {
        sendMsg(client, ERR_USERONCHANNEL, nick + " " + chanName + " :is already on channel");
        return;
    }

    chan.invited.insert(fd); // 초대 목록에 추가
    sendMsg(client, RPL_INVITING, nick + " " + chanName);
    std::string msg = ":" + client.nick + " INVITE " + nick + " :" + chanName + "\r\n";
    send(fd, msg.c_str(), msg.size(), 0);
}

void Server::handleTopic(Client &client, std::istringstream &iss)
{
    std::string chanName;
    iss >> chanName;
    
    if (chanName.empty()) {
        sendMsg(client, ERR_NEEDMOREPARAMS, "TOPIC :Not enough parameters");
        return;
    }

    std::map<std::string, Channel>::iterator it = _channels.find(chanName);
    if (it == _channels.end()) {
        sendMsg(client, ERR_NOSUCHCHANNEL, chanName + " :No such channel");
        return;
    }
    Channel &chan = it->second;

    // 1. 뒤에 인자가 있는지 확인 (조회 vs 변경 결정)
    std::string rawRemaining;
    std::getline(iss, rawRemaining);

    if (rawRemaining.empty()) {
        // [조회 로직] 인자가 아예 없는 경우: TOPIC #chan
        if (chan.topic.empty())
            sendMsg(client, RPL_NOTOPIC, chanName + " :No topic is set");
        else
            sendMsg(client, RPL_TOPIC, chanName + " :" + chan.topic);
        return;
    }

    // [변경 로직] 인자가 있는 경우: TOPIC #chan :new topic
    // 앞서 구현하신 PRIVMSG와 동일한 트리밍 로직 적용
    size_t first_not_space = rawRemaining.find_first_not_of(" ");
    if (first_not_space != std::string::npos)
        rawRemaining.erase(0, first_not_space);

    if (!rawRemaining.empty() && rawRemaining[0] == ':')
        rawRemaining.erase(0, 1);

    // 권한 체크 (+t 모드일 때)
    if (chan.topicRestricted && chan.operators.find(client.fd) == chan.operators.end()) {
        sendMsg(client, ERR_CHANOPRIVSNEEDED, chanName + " :You're not channel operator");
        return;
    }

    // 주제 업데이트 및 전체 브로드캐스트 (본인 포함)
    chan.topic = rawRemaining;
    std::string msg = ":" + client.nick + " TOPIC " + chanName + " :" + chan.topic + "\r\n";
    broadcastToChannel(chan, msg); // 채널 내 모든 유저에게 알림
}
