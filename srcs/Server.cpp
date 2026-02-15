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

void Server::handleClientData(int fd) {
    char buf[512]; // IRC 최대 길이 미만
    int bytes = recv(fd, buf, sizeof(buf) - 1, 0);

    if (bytes == 0) {
		// 연결 종료
		std::cout << "Client disconnected: " << fd << std::endl;
		removeClient(fd);
		return;
	}
	if (bytes < 0) {
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
		return;
	}
	
    client.buffer += buf; // 기존 버퍼에 누적
	
	size_t pos;

	while ((pos = client.buffer.find("\n")) != std::string::npos) {
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

    if (cmd == "PASS") {
        std::string pass;
        iss >> pass;
        if (pass != _password) {
            sendError(client, "Invalid password");
            return;
        }
        client.passOk = true;
    }
    else if (cmd == "NICK") {
        std::string nick;
        iss >> nick;
        if (nick.empty()) {
            sendError(client, "Nickname is required");
            return;
        }
        client.nick = nick;
    }
    else if (cmd == "USER") {
        if (!client.passOk) {
            sendError(client, "You must send PASS first");
            return;
        }
        if (client.nick.empty()) {
            sendError(client, "You must set NICK first");
            return;
        }

        std::string username;
        iss >> username;
        if (username.empty()) {
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
    else {
        if (!client.authed) {
            sendError(client, "You must register first");
        }
        else {
            // 이후 PRIVMSG, JOIN 등 처리
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

void Server::removeClient(int fd) {
    // pollfd에서 제거
    for (size_t i = 0; i < _pfds.size(); ++i) {
        if (_pfds[i].fd == fd) {
            _pfds.erase(_pfds.begin() + i);
            break;
        }
    }

    // 클라이언트 맵에서 제거
    _clients.erase(fd);

    // 소켓 닫기
    close(fd);
}