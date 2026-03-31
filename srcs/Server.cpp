#include "Server.hpp"

bool Server::stopFlag = false;

void handleSignal(int sig)
{
    (void)sig;
    Server::stopFlag = true;
}

void Server::shutdown()
{
    std::cout << "\nShutting down server...\n";

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

Server::Server(int port, const std::string& password) : _password(password)
{

    _listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (_listenFd < 0)
        throw std::runtime_error("socket failed");
    int yes = 1;
    setsockopt(_listenFd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(_listenFd, (sockaddr*)&addr, sizeof(addr)) < 0)
        throw std::runtime_error("bind failed");
    if (listen(_listenFd, 128) < 0)
        throw std::runtime_error("listen failed");

    fcntl(_listenFd, F_SETFL, O_NONBLOCK);

    pollfd pfd;
	memset(&pfd, 0, sizeof(pfd));
    pfd.fd = _listenFd;
    pfd.events = POLLIN;
    _pfds.push_back(pfd);
}

void Server::run()
{

    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, handleSignal);
    signal(SIGTERM, handleSignal);
    while (!stopFlag)
	{
		int poll_res = poll(&_pfds[0], _pfds.size(), -1);
		if (poll_res < 0 && errno == EINTR)
            continue;
        if (poll_res < 0)
            throw std::runtime_error("poll failed");

        for (size_t i = 0; i < _pfds.size(); )
        {
            bool client_removed = false;
            if (_pfds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) 
            {
                if (_pfds[i].fd == _listenFd) 
                    throw std::runtime_error("Server socket error");
                else 
                {
                    std::cout << "Socket error/hup on fd " << _pfds[i].fd << std::endl;
                    removeClient(_pfds[i].fd);
                    client_removed = true;
                }
            }
            else if (_pfds[i].revents & POLLIN)
            {
                if (_pfds[i].fd == _listenFd)
                    acceptNewClient();
                else
                {
                    int current_fd = _pfds[i].fd;
                    handleClientData(_pfds[i].fd);
                    if (_clients.find(current_fd) == _clients.end())
                        client_removed = true;
                }
            }
            if (!client_removed && (_pfds[i].revents & POLLOUT))
            {
                Client &client = _clients[_pfds[i].fd];
                if (!client.msgBuffer.empty())
                {
                    int sent = send(client.fd, client.msgBuffer.c_str(), client.msgBuffer.size(), 0);
                    if (sent > 0)
                        client.msgBuffer.erase(0, sent);
                    else if (sent < 0)
                    {
                        if (errno != EWOULDBLOCK && errno != EAGAIN)
                        {
                            removeClient(_pfds[i].fd);
                            client_removed = true;
                            std::cerr << "Send error on fd " << client.fd << std::endl;
                        }
                    }
                    if (client.msgBuffer.empty())
                        _pfds[i].events &= ~POLLOUT;
                }
            }
            if (client_removed)
                continue;
            ++i;
        }
    }
	this->shutdown();
}

void Server::acceptNewClient() {
    sockaddr_in clientAddr;
    socklen_t len = sizeof(clientAddr);
    int clientFd = accept(_listenFd, (sockaddr*)&clientAddr, &len);
    if (clientFd < 0)
        return;

    fcntl(clientFd, F_SETFL, O_NONBLOCK);

    pollfd pfd;
	memset(&pfd, 0, sizeof(pfd));
    pfd.fd = clientFd;
    pfd.events = POLLIN;
    _pfds.push_back(pfd);
    _clients.insert(std::make_pair(clientFd, Client(clientFd)));
    std::cout << "New client connected: " << clientFd << std::endl;
}

void Server::handleClientData(int fd)
{
    char buf[512];
    int bytes = recv(fd, buf, sizeof(buf) - 1, 0);

    if (bytes == 0)
    {
		std::cout << "Client disconnected: " << fd << std::endl;
		removeClient(fd);
		return;
	}
	if (bytes < 0)
    {
		if (errno == EWOULDBLOCK || errno == EAGAIN)
			return;
		std::cerr << "recv error on fd " << fd << std::endl;
		removeClient(fd);
		return;
	}

    std::string filtered;
	filtered.reserve(bytes);
	for (int i = 0; i < bytes; ++i)
	{
    	unsigned char c = buf[i];
    	if ((c >= 32 && c <= 126) || c == '\r' || c == '\n')
        	filtered += c;
	}
    Client &client = _clients[fd];
	const size_t MAX_BUFFER = 4096;

	
	if (client.buffer.size() + filtered.size() > MAX_BUFFER)
	{
		std::cerr << "Buffer overflow from fd " << fd << std::endl;
		removeClient(fd);
		return;
	}
	client.buffer.append(filtered);
	size_t pos;
    while ((pos = client.buffer.find("\n")) != std::string::npos)
    {
        std::string line = client.buffer.substr(0, pos);
        client.buffer.erase(0, pos + 1);

        while (!line.empty() && (line[line.size() - 1] == '\r' || line[line.size() - 1] == '\n'))
            line.erase(line.size() - 1);

        if (!line.empty())
		{
            std::cout << "Received: " << line << std::endl;
            processCommand(client, line);
        }
    }
}

void Server::processCommand(Client &client, const std::string &message) {
    std::istringstream iss(message);
    std::string cmd;
    iss >> cmd;

    if (cmd.empty())
        return;
    for (size_t i = 0; i < cmd.length(); ++i)
        {cmd[i] = std::toupper(static_cast<unsigned char>(cmd[i]));}
	if (cmd == "CAP")
		return ;
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
	else if (cmd == "PING")
	{
		std::string token;
		iss >> token;
		if (token.empty())
		{
			sendMsg(client, ERR_NEEDMOREPARAMS, "PING :Not enough parameters");
			return;
		}
		handlePing(client, token);
	}
    else if (cmd == "NICK")
    {
        if (!client.passOk)
		{
            sendMsg(client, ERR_NOTREGISTERED, ":You have not registered");
			return;
        }
        std::string newNick;
        iss >> newNick;

		while (!newNick.empty() && (newNick[newNick.size()-1] < 32))
        	newNick.erase(newNick.size()-1);
		if (newNick.empty())
			return;
        if (!isValidNick(newNick, client))
            return;
		if (!client.nick.empty() && client.authed)
		{
			std::string nickMsg = ":" + client.nick + "!" + client.user + "@127.0.0.1 NICK " + newNick + "\r\n";
			broadcastToRelatedClients(client, nickMsg);
		}
		client.nick = newNick;
		if (!client.user.empty() && !client.authed)
		{
        	client.authed = true;
			sendWelcome(client);
		}

    }
    else if (cmd == "USER")
    {
        if (!client.passOk)
		{
            sendMsg(client, ERR_NOTREGISTERED, ":You have not registered");
			return;
        }
        std::string username, hostname, servername, realname;
        if (!(iss >> username >> hostname >> servername)) {
            sendMsg(client, ERR_NEEDMOREPARAMS, "USER :Not enough parameters");
            return;
        }
        std::getline(iss, realname);
		size_t first = realname.find_first_not_of(" \t\r\n");
    	if (first != std::string::npos)
		{
        	realname = realname.substr(first);
        	if (realname[0] == ':')
            	realname = realname.substr(1);
    	}
        else
        {
            sendMsg(client, ERR_NEEDMOREPARAMS, "USER :Not enough parameters");
            return;
        }

        if (!isValidUser(username, client))
            return ;
        client.user = username;
		if (hostname == "0" || hostname == "*")
			hostname = "127.0.0.1";
        client.hostname = hostname;
        client.realname = realname;
        client.realname = realname;
		if (!client.nick.empty() && !client.authed)
		{
        	client.authed = true;
			sendWelcome(client);
		}

    }
    else
    {
        if (!client.authed)
        {
            sendMsg(client, ERR_NOTREGISTERED, ":You have not registered");
            return;
        }
        if (cmd == "JOIN")
			handleJoin(client, iss);
		else if (cmd == "PRIVMSG")
			handlePrivmsg(client, iss);
        else if (cmd == "MODE")
        {
            std::string chanName;
            iss >> chanName;
            std::string modeStr;
            iss >> modeStr;
			if (chanName == client.nick && !modeStr.empty() && modeStr == "+i")
			{
				std::string msg = ":" + client.nick + "!" + client.user + "@" + client.hostname + 
                      " MODE " + client.nick + " :+i\r\n";
				client.msgBuffer += msg;
    			for (size_t i = 0; i < _pfds.size(); ++i)
    			{
        			if (_pfds[i].fd == client.fd)
        			{
        	    		_pfds[i].events |= POLLOUT;
        	    		break;
        			}
    			}
			}
			else 
            	handleMode(client, chanName, modeStr, iss);
        }
        else if (cmd == "KICK")
            handleKick(client, iss);
        else if (cmd == "INVITE")
            handleInvite(client, iss);
        else if (cmd == "TOPIC")
            handleTopic(client, iss);
        else if (cmd == "WHO")
		{
    		std::string target;
    		iss >> target;
    		std::string whoReply = target + " " + client.user + " " + client.hostname + " ircserv " + client.nick + " H :0 " + client.realname;
    		sendMsg(client, RPL_NAMREPLY, whoReply);
    		sendMsg(client, RPL_ENDOFWHO, target + " :End of /WHO list");
		}
		else if (cmd == "WHOIS")
		{
    		std::string target;
    		iss >> target;
    		sendMsg(client, RPL_ENDOFWHOIS, target + " :End of /WHOIS list");
		}
		else
            sendMsg(client, ERR_UNKNOWNCOMMAND, cmd + " :Unknown command");
    }
}

void Server::sendWelcome(Client &client)
{
	std::string msg = ":ircserv 001 " + client.nick + 
                      " :Welcome to the Internet Relay Network " + client.nick + "!" + client.user + "@" + client.hostname + "\r\n";
	msg += ":ircserv 002 " + client.nick + " :Your host is ircserv, running version 1.0\r\n";
    msg += ":ircserv 003 " + client.nick + " :This server was created Mon Mar 30 2026\r\n";
    msg += ":ircserv 004 " + client.nick + " ircserv 1.0 i oitkl\r\n";
	client.msgBuffer += msg;
    for (size_t i = 0; i < _pfds.size(); ++i)
    {
    	if (_pfds[i].fd == client.fd)
        {
            _pfds[i].events |= POLLOUT;
            break;
        }
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
    if (!std::isalpha(nick[0]) && std::string("[]\\`^{}|_").find(nick[0]) == std::string::npos)
    {
        sendMsg(client, ERR_ERRONEUSNICKNAME, nick + " :Erroneus nickname");
        return (false);
    }
    for (size_t i = 1; i < nick.length(); ++i)
	{
        char c = nick[i];
        
        if (!std::isalnum(c) && std::string("[]\\`^{}|-_").find(c) == std::string::npos)
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
    std::string response = ":ircserv " + code + " " + nick + " " + msg + "\r\n";
    if (response.size() > 510)
        response = response.substr(0, 510) + "\r\n";

    client.msgBuffer += response;
    for (size_t i = 0; i < _pfds.size(); ++i)
    {
        if (_pfds[i].fd == client.fd)
        {
            _pfds[i].events |= POLLOUT;
            break;
        }
    }
}

void Server::removeClient(int fd)
{
    std::map<int, Client>::iterator clientIt = _clients.find(fd);
    if (clientIt != _clients.end())
    {
        Client &client = clientIt->second;
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
        for (size_t i = 0; i < emptyChannels.size(); ++i)
            _channels.erase(emptyChannels[i]);
    }
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

void Server::broadcastToRelatedClients(Client &client, const std::string &msg)
{
    std::set<int> targetFds;
    targetFds.insert(client.fd);

    for (std::set<std::string>::iterator it = client.joinedChannels.begin(); 
         it != client.joinedChannels.end(); ++it) 
    {
        std::map<std::string, Channel>::iterator mit = _channels.find(*it);
        
        if (mit != _channels.end()) 
        {
            Channel &chan = mit->second;
            for (std::set<int>::iterator cit = chan.clients.begin(); 
                 cit != chan.clients.end(); ++cit) 
            {
                targetFds.insert(*cit);
            }
        }
    }

    for (std::set<int>::iterator tit = targetFds.begin(); 
         tit != targetFds.end(); ++tit) 
    {
        int fd = *tit;
        _clients[fd].msgBuffer += msg;

        for (size_t i = 0; i < _pfds.size(); ++i)
        {
            if (_pfds[i].fd == fd)
            {
                _pfds[i].events |= POLLOUT;
                break;
            }
        }
    }
}

void Server::broadcastToChannel(Channel &chan, const std::string &msg)
{
	for (std::set<int>::iterator it = chan.clients.begin();
	     it != chan.clients.end(); )
	{
		std::set<int>::iterator current = it;
		++it;

		int fd = *current;
        
		_clients[fd].msgBuffer += msg;

        for (size_t i = 0; i < _pfds.size(); ++i)
        {
            if (_pfds[i].fd == fd)
            {
                _pfds[i].events |= POLLOUT;
                break;
            }
        }
	}
}

void Server::handlePing(Client &client, std::string token)
{
    std::string response = ":ircserv PONG ircserv : " + token + "\r\n";
	client.msgBuffer += response;
	for (size_t i = 0; i < _pfds.size(); ++i)
    {
        if (_pfds[i].fd == client.fd)
        {
            _pfds[i].events |= POLLOUT;
            break;
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
    if (chan.clients.find(client.fd) != chan.clients.end())
    {
        sendMsg(client, ERR_USERONCHANNEL, channelName + " :is already on channel");
        return;
    }
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
    if (chan.userLimit > 0 && (size_t)chan.clients.size() >= chan.userLimit)
    {
        sendMsg(client, ERR_CHANNELISFULL, channelName + " :Cannot join channel (+l)");
        return;
    }
    chan.clients.insert(client.fd);
    client.joinedChannels.insert(channelName);

    if (chan.clients.size() == 1)
        {chan.operators.insert(client.fd);}
        
	std::string joinMsg = ":" + client.nick + "!" + client.user + "@" + client.hostname + " JOIN " + channelName + "\r\n";
    broadcastToChannel(chan, joinMsg);

	if (chan.topic.empty())
		sendMsg(client, RPL_NOTOPIC, channelName + " :No topic is set");
    else
        sendMsg(client, RPL_TOPIC, channelName + " :" + chan.topic);

    std::string names = "";
	for (std::set<int>::iterator it = chan.clients.begin(); it != chan.clients.end(); ++it)
	{
	    if (it != chan.clients.begin()) names += " ";
	    if (chan.operators.find(*it) != chan.operators.end())
	        names += "@";
	    names += getNickByFd(*it);
	}
	sendMsg(client, RPL_NAMREPLY, "= " + channelName + " :" + names);
	sendMsg(client, RPL_ENDOFNAMES , channelName + " :End of /NAMES list");
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
            _clients[fd].msgBuffer += msg;
            for (size_t i = 0; i < _pfds.size(); ++i)
            {
                if (_pfds[i].fd == fd)
                {
                    _pfds[i].events |= POLLOUT;
                    break;
                }
            }
        }
    }
}


void Server::handlePrivmsg(Client &client, std::istringstream &iss)
{
    std::string target;
    if (!(iss >> target))
    {
        sendMsg(client, ERR_NEEDMOREPARAMS, "PRIVMSG :Not enough parameters");
        return;
    }

    std::string message;
    std::getline(iss, message);
	
    size_t first = message.find_first_not_of(" \r\n");
    if (first == std::string::npos)
        message = "";
    else
    {
        size_t last = message.find_last_not_of(" \r\n");
        message = message.substr(first, (last - first + 1));
    }

    if (message.empty())
    {
        sendMsg(client, ERR_NEEDMOREPARAMS, "PRIVMSG :Not enough parameters");
        return;
    }
    if (!message.empty() && message[0] == ':')
	{
        message.erase(0, 1);
	}
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
    int targetFd = getFdByNick(target);
    if (targetFd != -1)
    {
        std::string full = ":" + client.nick + " PRIVMSG " + target + " :" + message + "\r\n";
        _clients[targetFd].msgBuffer += full;
        for (size_t i = 0; i < _pfds.size(); ++i)
        {
            if (_pfds[i].fd == targetFd)
            {
                _pfds[i].events |= POLLOUT;
                break;
            }
        }
        return ;
    }
    sendMsg(client, ERR_NOSUCHNICK, target + " :No such nick/channel");
    return;
}

void Server::handleMode(Client &client, const std::string &chanName, const std::string &modeStr, std::istringstream &iss)
{
    if (chanName.empty())
    {
        sendMsg(client, ERR_NEEDMOREPARAMS, "MODE :Not enough parameters");
        return;
    }
    std::map<std::string, Channel>::iterator it = _channels.find(chanName);
    if (it == _channels.end())
    {
        sendMsg(client, ERR_NOSUCHCHANNEL, chanName + " :No such channel");
        return;
    }
    Channel &chan = it->second;
	if (modeStr.empty())
	{
			bool isMember = (chan.clients.find(client.fd) != chan.clients.end());
			sendMsg(client, RPL_CHANNELMODEIS, chanName + " " + chan.getModes(isMember));
			sendMsg(client, RPL_CREATIONTIME, chanName + " " + chan.getCreationTime());
			return;
	}
	if (modeStr == "b")
		return ;
    if (chan.operators.find(client.fd) == chan.operators.end())
    {
        sendMsg(client, ERR_CHANOPRIVSNEEDED, chanName + " :You're not channel operator");
        return;
    }
    std::string appliedMode = "";
    std::string appliedParams = "";
    char action = '+';
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
                    if (!(iss >> key)) {
                        sendMsg(client, ERR_NEEDMOREPARAMS, "MODE +k :Not enough parameters");
                        continue; 
                    }
                    if (key.length() > 32)
                    {
                        sendMsg(client, ERR_INVALIDMODEPARAMS, chanName + " k " + key + " :Password is too long (max 32)");
                        continue;
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
			case 'b':
				break;
            default:
                sendMsg(client, ERR_UNKNOWNMODE, std::string(1, m) + " :is unknown mode char to me");
                break;
        }
    }

    if (!appliedMode.empty())
	{
        std::string finalMsg = ":" + client.nick + "!" + client.user + "@" + client.hostname + 
                          " MODE " + chanName + " " + appliedMode + appliedParams + "\r\n";
        broadcastToChannel(chan, finalMsg);
    }
}

int Server::getFdByNick(const std::string &nick)
{
    std::map<int, Client>::iterator it = _clients.begin();
    for (; it != _clients.end(); ++it)
    {
        if (it->second.nick == nick)
            return it->first;
    }
    return -1;
}

std::string Server::getNickByFd(int fd)
{
    std::map<int, Client>::iterator it = _clients.find(fd);
    if (it != _clients.end())
    {
        return it->second.nick.empty() ? "*" : it->second.nick;
    }
    return "Unknown";
}

void Server::handleKick(Client &client, std::istringstream &iss)
{
    std::string chanName, nick, reason;
	
	if (!(iss >> chanName >> nick))
	{
		sendMsg(client, ERR_NEEDMOREPARAMS, "KICK :Not enough parameters");
		return;
	}

	std::getline(iss, reason);
	if (reason.empty() || reason.find_first_not_of(" ") == std::string::npos)
        reason = " :Kicked by operator";
	else
        reason = " :" + reason.substr(reason.find_first_not_of(" "));
		

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

    std::string msg = ":" + client.nick + "!" + client.user + "@" + client.hostname + 
                      " KICK " + chanName + " " + nick + reason + "\r\n";
    broadcastToChannel(chan, msg);

    chan.clients.erase(fd);
    chan.operators.erase(fd);
    chan.invited.erase(fd);
    _clients[fd].joinedChannels.erase(chanName);

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
    if (chan.operators.find(client.fd) == chan.operators.end()) {
        sendMsg(client, ERR_CHANOPRIVSNEEDED, chanName + " :You're not channel operator");
        return;
    }
    int fd = getFdByNick(nick);
    if (fd == -1) {
        sendMsg(client, ERR_NOSUCHNICK, nick + " :No such nick");
        return;
    }
    if (chan.clients.find(fd) != chan.clients.end()) {
        sendMsg(client, ERR_USERONCHANNEL, nick + " " + chanName + " :is already on channel");
        return;
    }

    chan.invited.insert(fd);
    sendMsg(client, RPL_INVITING, nick + " " + chanName);
	std::string msg = ":" + client.nick + "!" + client.user + "@" + client.hostname + 
                  " INVITE " + nick + " :" + chanName + "\r\n";
	_clients[fd].msgBuffer += msg;
    for (size_t i = 0; i < _pfds.size(); ++i) {
        if (_pfds[i].fd == fd) {
            _pfds[i].events |= POLLOUT;
            break;
        }
    }
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

	if (chan.clients.find(client.fd) == chan.clients.end())
	{
    	sendMsg(client, ERR_NOTONCHANNEL, chanName + " :You're not on that channel");
    	return;
	}
    std::string rawRemaining;
    std::getline(iss, rawRemaining);

    if (rawRemaining.empty())
	{
        if (chan.topic.empty())
            sendMsg(client, RPL_NOTOPIC, chanName + " :No topic is set");
        else
            sendMsg(client, RPL_TOPIC, chanName + " :" + chan.topic);
        return;
    }
    size_t first_not_space = rawRemaining.find_first_not_of(" ");
    if (first_not_space != std::string::npos)
        rawRemaining.erase(0, first_not_space);

    if (!rawRemaining.empty() && rawRemaining[0] == ':')
        rawRemaining.erase(0, 1);
    if (chan.topicRestricted && chan.operators.find(client.fd) == chan.operators.end())
	{
        sendMsg(client, ERR_CHANOPRIVSNEEDED, chanName + " :You're not channel operator");
        return;
    }

    chan.topic = rawRemaining;
    std::string msg = ":" + client.nick + "!" + client.user + "@" + client.hostname + 
                      " TOPIC " + chanName + " :" + chan.topic + "\r\n";
    broadcastToChannel(chan, msg);
}
