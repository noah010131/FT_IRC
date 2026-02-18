#include "Client.hpp"

Client::Client() : fd(-1), authed(false), passOk(false) {}

Client::Client(int fd) : fd(fd), authed(false), passOk(false) {}
