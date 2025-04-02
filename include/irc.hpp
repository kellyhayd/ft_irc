#pragma once

/* Standard Input/Output */
#include <iostream>

/* Standard Library */
#include <cstring>

/* String & Stream */
#include <string>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <fstream>

/* Algorithms & Utilities */
#include <iterator>
#include <algorithm>

/* Containers */
#include <map>
#include <list>
#include <vector>

/* Exception Handling */
#include <limits>
#include <signal.h>
#include <stdexcept>
#include <exception>

/* Time */
#include <ctime>

/* System & Networking */
#include <poll.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "Server.hpp"
#include "Client.hpp"
#include "Channel.hpp"
#include "MsgHandler.hpp"
#include "ChannelManager.hpp"

/* Macros */
# define MAX_PORT 65535 // must be 16-bit unsigned integer
# define MIN_PORT 1024 // 1-1023 are reserved ports, require root

/* Enumerations */ 

/* Error messages */
# define ERR_USAGE "Usage: ./ircserv <port> <password>"
# define ERR_PORT "Invalid port number"
# define ERR_PORT_NUMERIC "Port number must be numeric (1024 - 65535)"
# define ERR_INVALID_PASSWORD "Invalid password"
# define ERR_INVALID_PASSWORD_LENGTH "Password must be 1 characters long"

// Reply codes for server responses

// RPL MESSAGE MACROS
#define RPL_WELCOME(client) std::string(":") + SERVER_NAME + " 001 " + client.nickname() + " :Welcome to the IRC Network, " + client.nickname() + "!" + client.username() + "@" + client.hostname() + "\r\n"
#define RPL_YOURHOST(client) std::string(":") + SERVER_NAME + " 002 " + client.nickname() + " :Your host is " + SERVER_NAME + ", running version 1.0\r\n"
#define RPL_CREATED(client) std::string(":") + SERVER_NAME + " 003 " + client.nickname() + " :This server was created, 2025-03-31\r\n"
#define RPL_MYINFO(client) std::string(":") + SERVER_NAME + " 004 " + client.nickname() + " " + SERVER_NAME + " 1.0 o itkol\r\n"
#define RPL_PASSWDMISMATCH(client) std::string(":") + SERVER_NAME + " 464 " + client.nickname() + " :Password incorrect\r\n"
#define RPL_YOUROPER(client) std::string(":") + SERVER_NAME + " 381 " + client.nickname() + " :You are now an IRC operator\r\n"
#define RPL_NOOPERHOST(client) std::string(":") + SERVER_NAME + " 491 " + client.nickname() + " :No O-lines for your host\r\n"

#define PONG std::string("PONG ") + SERVER_NAME + "\r\n"

/* Structures */
typedef std::pair<int, Client *>	client_pair_t;
typedef std::map<int, Client *>		clients_t;

/* Function prototypes */ 
int     checkInput(int ac, char **av);
int     errMsgVal(int detail, const std::string& str, int code);
int     errMsg(const std::string& detail, const std::string& str, int code);
std::vector<std::string> split(const std::string& str, char delimiter);

void	        handleCtrlD(void);
std::string     getCurrentTime(void);
std::string     intToString(int value);
std::string     uintToString(unsigned int value);
int             isDigits(const std::string& s);
int             isValidPort(const std::string& s);
unsigned int    getUnsignedInt(const std::string& prompt);
void            printStr(const std::string& text, const std::string& colour);
void 			sendMSG(int fd, std::string RPL);

// Logging
void info(const std::string& message);
void error(const std::string& message);
void warning(const std::string& message);

//Command
enum Command
{
    USER,
    NICK,
    JOIN,
    PART,
    INVITE,
    KICK,
    MODE,
    TOPIC,
    PING,
    QUIT,
    OPER,
    PRIVMSG,
    UNKNOWN
};
std::map<std::string, Command> createCommandMap();
Command getCommandType(const std::string& cmd);

/* Colours */
#define RESET   "\e[0m"
#define RED     "\e[31m"
#define GREEN   "\e[32m"
#define YELLOW  "\e[33m"
#define BLUE    "\e[34m"
#define PURPLE  "\e[35m"
#define CYAN    "\e[36m"
#define WHITE   "\e[37m"
#define BRED    "\e[1;31m"
#define BGREEN  "\e[1;32m"
#define BYELLOW "\e[1;33m"
#define BBLUE   "\e[1;34m"
#define BPURPLE "\e[1;35m"
#define BCYAN   "\e[1;36m"
#define BGR     "\e[41m"
#define BGG     "\e[42m"
#define BGY     "\e[43m"
#define BGB     "\e[44m"
#define BGP     "\e[45m"
#define BGC     "\e[46m"
#define BGW     "\e[47m"
