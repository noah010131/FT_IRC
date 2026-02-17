#pragma once
#include <map>
#include <vector>
#include <string>
#include <poll.h>

#include "Client.hpp"
#include "Channel.hpp"

class Server {
private:
    int _listenFd;
    std::vector<struct pollfd> _pfds;
    std::map<int, Client> _clients;
    std::map<std::string, Channel> _channels;
    std::string _password;

public:
    Server(int port, const std::string& password);
    void run();

private:
    void acceptNewClient();
    void handleClientData(int fd);
	void processCommand (Client &client, const std::string &message);
    void removeClient(int fd);
    void sendError(Client &client, const std::string &msg);
    void handleJoin(Client &client, std::istringstream &iss);
    Channel& getOrCreateChannel(const std::string &name);
    void broadcastToChannel(Channel &chan, const std::string &msg);

	void handlePrivmsg(Client &client, std::istringstream &iss);
	Client* findClientByNick(const std::string &nick);
	void sendToChannel(const std::string &channel, const std::string &msg, int exceptFd);
    void handleMode(Client &client, const std::string &chanName, const std::string &modeStr, std::istringstream &iss);
    int getFdByNick(const std::string &nick);
    void handleKick(Client &client, std::istringstream &iss);
    void handleInvite(Client &client, std::istringstream &iss);
    void handleTopic(Client &client, std::istringstream &iss);

};
