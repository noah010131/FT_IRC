This project has been created as part of the 42 curriculum by chanypar.

# ft_irc: Internet Relay Chat Server
## Description
ft_irc is a custom-built Internet Relay Chat (IRC) server developed in C++98. The goal of this project is to recreate the core functionalities of an IRC server, enabling real-time communication between multiple clients using the IRC protocol.

The project focuses on:

Socket Programming: Managing multiple simultaneous connections using non-blocking I/O and I/O multiplexing (poll(), select(), or epoll()).

Protocol Implementation: Adhering to the RFC 2812 (and related) standards to handle client authentication, channel management, and private messaging.

Concurrency: Handling the complexity of partial data reception and command buffering for multiple users without data corruption.

## Instructions
Compilation

The project includes a Makefile that compiles the source files with the required flags (-Wall -Wextra -Werror -std=c++98). To compile the server, run:

``` Bash
make
Execution ```

The server requires two arguments: the port to listen on and the password required for clients to connect.

Bash
./ircserv <port> <password>
Port: A valid port number (usually 6660–6669).

Password: A string used by clients to authenticate during the connection handshake.

Connecting to the Server

You can use any standard IRC client to connect. For example, using irssi:

Bash
/connect localhost <port> <password> <nickname>
Alternatively, for low-level testing, you can use nc (netcat):

Bash
nc -C localhost <port>
PASS <password>
NICK <nickname>
USER <username> 0 * :<realname>
Resources
Documentation & References

RFC 2812: The primary reference for the Internet Relay Chat: Client Protocol.

Beej's Guide to Network Programming: An essential resource for understanding Sockets and Network I/O.

Modern IRC Specifications: Information regarding IRCv3 capabilities.

Use of AI

In compliance with the 42 curriculum guidelines, AI (Gemini 3 Flash) was utilized in the following capacity:

Debugging: Assisting in interpreting complex compiler errors (e.g., -Wmisleading-indentation) and Git corruption issues.

Refactoring: Converting modern C++11/17 functions (like std::to_string) into C++98-compliant code using std::stringstream.

Edge Case Identification: Brainstorming potential issues with IRC command parsing, specifically regarding the handling of control characters (ANSI escape sequences) in raw socket buffers.

Template Generation: Assisting in the structural layout of this documentation.
