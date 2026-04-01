#ifndef SERVER_HPP
# define SERVER_HPP

# include <iostream>
# include <fstream>
# include <string>
# include <vector>
# include <map>
# include <cstring>
# include <cstdlib>
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include <unistd.h>
# include <poll.h>
# include <signal.h>
# include <fcntl.h>
# include <sstream>
# include <errno.h>
# include "Client.hpp"
# include "Channel.hpp"
# include "ServerMsg.hpp"
# include "ReplyCode.hpp"
# include "Cmd/Cmd.hpp"

# define MAX_CLIENTS 20
# define BUFFER_SIZE 513

# define PORT_MAX 65535
# define PORT_MIN 0
# define PORT_MAX_LEN 5
# define PASSWORD_MAX_LEN 10

# define PATH_GOAT "./conf/goat.txt"
# define PATH_MOTD "./conf/motd.txt"
# define SERVER_NAME "ircserv"

# define CRLF "\r\n"
# define PREFIX_SERVER ":ircserv"
# define PREFIX_USER(nickname, username, hostname) 	":" + nickname + "!" + username + "@" + hostname

# define WAIT_FOR_PING_MAX 250

# define LOG_ERR -1
# define LOG_SERVER 0
# define LOG_INPUT 1
# define LOG_OUTPUT 2

class IrcServer {
private:
	int										_fd;
	int										_port;
	std::string								_password;
	const std::string						_servername;
	time_t									_startTime;

	std::vector<struct pollfd>				_fds;
	std::map<int, Client*>					_clients;
	std::map<std::string, Channel *>		_channels;
	std::map<std::string, Client*>			nickNameClientMap;
	std::vector<int>						_fdsToRemove;

public:
	IrcServer();
	IrcServer(const char *port, const char *password);
	~IrcServer();
	
	std::string								getPassword();
	const std::string						getName() const;
	time_t									getStartTime() const;
	Client*									getClient(const std::string& nickname);
	Client*									getClient(int client_fd);
	const std::map<std::string, Client*>&	getNickNameClientMap() const;
	std::map<std::string, Channel*>&		getChannels();
	void									setChannels(const std::string& channelName, const std::string& key, const char& mode);

	void									init();
	void									acceptClient();
	void									run();
	void									handleSocketRead(int fd);
	void									handleSocketWrite(int fd);
	void									checkPingTimeOut();
	std::string								makeMsg(const std::string& prefix, const std::string& msg);
	void									castMsg(int client_fd, const std::string message);
	void									broadcastMsg(const std::string& message, Channel* channel, int senderFd);
	void									removeChannel(const std::string channelName);
	void									removeClientFd(int client_fd);
	void									removeClientFromServer(Client* client);
	void									addClientByNickname(const std::string& nickname, Client* client);
	void									updateClients(Client* client);
	void									updateNickNameClientMap(const std::string& oldNick, const std::string& newNick, Client* client);
	void									enablePollOutEvent(int client_fd);
	void									serverLog(int fd, int log_type, std::string log_color, std::string msg);
	std::string								formatDateToString(time_t time);
	std::string								intToString(int num);
	void									printGoat();

	/* exception */
	class ServerException : public std::exception {
	private:
		std::string	msg;
	public:
		ServerException(std::string str) : msg(str) {};
		virtual ~ServerException() throw() {};
		virtual const char*	 what() const throw();
	};
};

#endif
