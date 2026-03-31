
<p align="center">
  <a href="https://42.fr/en/homepage/" target="blank">
    <img src="https://upload.wikimedia.org/wikipedia/commons/8/8d/42_Logo.svg" width="150" alt="42 Logo" />
  </a>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Campus-Paris-000000?style=for-the-badge&logo=target" />
  <img src="https://img.shields.io/badge/Project-ft__irc-E96432?style=for-the-badge&logo=c%2B%2B" />
  <img src="https://img.shields.io/badge/Standard-C++98-00599C?style=for-the-badge&logo=c%2B%2B" />
</p>

<p align="center">
  <b>A Multithreaded-like IRC Server based on I/O Multiplexing.</b><br>
  Implementing a fully functional Internet Relay Chat server from scratch using <code>poll()</code> or <code>epoll/kqueue</code>.
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Socket-Network-orange?style=flat-square&logo=dynamic-365">
  <img src="https://img.shields.io/badge/I/O-Multiplexing-yellow?style=flat-square">
  <img src="https://img.shields.io/badge/TCP/IP-Protocol-red?style=flat-square">
  <img src="https://img.shields.io/badge/RFC-1459/2812-lightgrey?style=flat-square">
</p>

---

# ft_irc: Internet Relay Chat Server


## Description
ft_irc is a custom-built Internet Relay Chat (IRC) server developed in C++98. The goal of this project is to recreate the core functionalities of an IRC server, enabling real-time communication between multiple clients using the IRC protocol.

The project focuses on:

Socket Programming: Managing multiple simultaneous connections using non-blocking I/O and I/O multiplexing (poll(), select(), or epoll()).

Protocol Implementation: Adhering to the RFC 2812 (and related) standards to handle client authentication, channel management, and private messaging.

Concurrency: Handling the complexity of partial data reception and command buffering for multiple users without data corruption.

## Instructions
### Compilation

The project includes a Makefile that compiles the source files with the required flags (-Wall -Wextra -Werror -std=c++98). To compile the server, run:

```
Bash
make
Execution
```

The server requires two arguments: the port to listen on and the password required for clients to connect.

```
Bash
./ircserv <port> <password>
Port: A valid port number (usually 6660–6669).
```

Password: A string used by clients to authenticate during the connection handshake.

Connecting to the Server

You can use any standard IRC client to connect. For example, using irssi:

```
Bash
/connect localhost <port> <password> <nickname>
```
Alternatively, for low-level testing, you can use nc (netcat):

```
Bash
nc -C localhost <port>
PASS <password>
NICK <nickname>
USER <username> 0 * :<realname>
```


## Resources
### Documentation & References

RFC 2812: The primary reference for the Internet Relay Chat: Client Protocol.

Beej's Guide to Network Programming: An essential resource for understanding Sockets and Network I/O.

Modern IRC Specifications: Information regarding IRCv3 capabilities.

### Use of AI

In compliance with the 42 curriculum guidelines, AI (Gemini 3 Flash) was utilized in the following capacity:

> **Debugging:** Assisting in interpreting complex compiler errors (e.g., -Wmisleading-indentation) and Git corruption issues.

> **Edge Case Identification:** Brainstorming potential issues with IRC command parsing, specifically regarding the handling of control characters (ANSI escape sequences) in raw socket buffers.

