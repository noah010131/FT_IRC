/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: chanypar <chanypar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/21 18:43:12 by chanypar          #+#    #+#             */
/*   Updated: 2026/02/21 18:43:13 by chanypar         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once
#include <map>
#include <vector>
#include <string>
#include <poll.h>
#include <csignal>

#include "Client.hpp"
#include "Channel.hpp"

#define RPL_WELCOME "001"
#define RPL_INVITING "341"

#define RPL_CHANNELMODEIS "324"
#define RPL_CREATIONTIME "329"

#define RPL_NOTOPIC "331"
#define RPL_TOPIC "332"

#define ERR_UNKNOWNCOMMAND "421"

#define ERR_NEEDMOREPARAMS "461"
#define ERR_ALREADYREGISTRED "462"
#define ERR_PASSWDMISMATCH "464"

#define ERR_NONICKNAMEGIVEN "431"
#define ERR_NICKNAMEINUSE "433"
#define ERR_ERRONEUSNICKNAME "432"

#define ERR_NOTREGISTERED "451"
#define ERR_NOSUCHNICK "401"

#define ERR_NOSUCHCHANNEL "403"
#define ERR_NOTONCHANNEL "442"
#define ERR_USERNOTINCHANNEL "441"
#define ERR_USERONCHANNEL "443"
#define ERR_CHANOPRIVSNEEDED "482"
#define ERR_BADCHANNELKEY "475"
#define ERR_INVITEONLYCHAN "473"
#define ERR_CHANNELISFULL "471"

#define ERR_UNKNOWNMODE "472"
#define ERR_NICKNAMEINUSE "433"
#define ERR_INVALIDMODEPARAMS "696"

class Server
{
private:
    int fd;
    std::vector<struct pollfd> _pfds;
    std::map<int, Client> _clients;
    std::map<std::string, Channel> _channels;
    std::string _password;


public:
    Server(int port, const std::string& password);

    void run();
    void shutdown();

    void acceptNewClient();
    void handleClientData(int fd);
	void processCommand (Client &client, const std::string &message);
    void removeClient(int fd);
    void sendMsg(Client &client, const std::string &code, const std::string &msg);
    
    Channel& getOrCreateChannel(const std::string &name);
    void broadcastToChannel(Channel &chan, const std::string &msg);
    void broadcastToRelatedClients(Client &client, const std::string &msg);
    void sendToChannel(const std::string &channel, const std::string &msg, int exceptFd);
    
    bool nickExists(const std::string &nick) const;
    bool isValidUser(const std::string& user, Client &client);
    bool isValidNick(const std::string& nick, Client &client);
    int getFdByNick(const std::string &nick);
	Client* findClientByNick(const std::string &nick);
    std::string getNickByFd(int fd);
    
	void handlePrivmsg(Client &client, std::istringstream &iss);
    void handleJoin(Client &client, std::istringstream &iss);
    void handleMode(Client &client, const std::string &chanName, const std::string &modeStr, std::istringstream &iss);
	void handlePing(Client &client, std::string token);
    void handleKick(Client &client, std::istringstream &iss);
    void handleInvite(Client &client, std::istringstream &iss);
    void handleTopic(Client &client, std::istringstream &iss);

    static bool stopFlag;
};
