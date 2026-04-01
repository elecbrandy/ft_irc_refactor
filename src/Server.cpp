#include "Server.hpp"
#include "Client.hpp"

IrcServer::IrcServer() {}

IrcServer::IrcServer(const char *port, const char *password)
: _servername(SERVER_NAME), _startTime(time(NULL)) {
	/* Check Port number */
	if (port == NULL || *port == '\0') {
		throw ServerException(ERR_PORT_NULL);
	}

	for (int i = 0; port[i] != '\0'; ++i) {
		if (!std::isdigit(static_cast<unsigned char>(port[i]))) {
			throw ServerException(ERR_PORT_DIGIT);
		}
	}

	int tmpPort = std::atoi(port);
	if (tmpPort < PORT_MIN || tmpPort > PORT_MAX || std::strlen(port) > PORT_MAX_LEN) {
		throw ServerException(ERR_PORT_RANGE);
	}
	this->_port = tmpPort;

	/* Check Password */
	if (password == NULL || *password == '\0') {
		throw ServerException(ERR_PASSWORD_NULL);
	}

	for (int i = 0; password[i] != '\0'; ++i) {
		if (!std::isalnum(static_cast<unsigned char>(password[i]))) {
			throw ServerException(ERR_PASSWORD_ALNUM);
		}
	}

	if (std::strlen(password) > PASSWORD_MAX_LEN) {
		throw ServerException(ERR_PASSWORD_SIZE);
	}
	this->_password = std::string(password);
}

IrcServer::~IrcServer() {
	/* Close server socket */
	close(this->_fd);

	/* Close & Delete Client resource */
	for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
		if (it->first != -1) {
			close(it->first);
		}
		delete it->second;
	}
	_clients.clear();
	_fds.clear();
	nickNameClientMap.clear();
}

/* getter */

std::string IrcServer::getPassword() {return this->_password;}

const std::string IrcServer::getName() const {return this->_servername;}

time_t IrcServer::getStartTime() const {return this->_startTime;}

#include <ctime>  // 추가 필요

std::string IrcServer::formatDateToString(time_t time) {
    struct tm *timeInfo = localtime(&time);  // std:: 제거
    char buffer[20];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeInfo);  // std:: 제거

    std::ostringstream oss;
    oss << buffer;
    return oss.str();
}



void IrcServer::init() {
	
	if (BUFFER_SIZE < 4)
		throw ServerException(ERR_BUFFER_SIZE);
		
	if ((_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
		throw ServerException(ERR_SOCKET_CREATION);
	}

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(_port);

	if (bind(_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
		throw ServerException(ERR_SOCKET_BIND);
	}

	if (listen(_fd, MAX_CLIENTS) == -1) {
		throw ServerException(ERR_SOCKET_LISTEN);
	}

	if (fcntl(_fd, F_SETFL, O_NONBLOCK) == -1) {
		throw ServerException(ERR_SET_NONBLOCKING);
	}

	struct pollfd server_poll_fd;
	server_poll_fd.fd = _fd;
	server_poll_fd.events = POLLIN;
	_fds.push_back(server_poll_fd);

	std::system("clear");
	printGoat(); // goat
	serverLog(this->_fd, LOG_SERVER, C_MSG, MSG_SERVER_INIT(intToString(this->_port)));
}

void IrcServer::acceptClient() {
	struct sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);
	int client_fd = accept(_fd, (struct sockaddr*)&client_addr, &client_len);
	if (client_fd == -1) {
		throw ServerException(ERR_SOCKET_CREATION);
	}

	if (_fds.size() - 1 >= MAX_CLIENTS) {
		close(client_fd);
		throw ServerException(ERR_ACCEPT_CLIENT);
	}

	if (fcntl(client_fd, F_SETFL, O_NONBLOCK) == -1) {
		close(client_fd);
		throw ServerException(ERR_CLIENT_NONBLOCKING);
	}

	struct pollfd client_poll_fd;
	client_poll_fd.fd = client_fd;
	client_poll_fd.events = POLLIN;
	_fds.push_back(client_poll_fd);

	Client* newClient = new Client(client_addr.sin_addr);
	newClient->setFd(client_fd);
	_clients[client_fd] = newClient;
	serverLog(this->_fd, LOG_SERVER, C_MSG, MSG_NEW_CONNECTED(intToString(client_fd)));
}

void IrcServer::checkPingTimeOut() {
	std::vector<Client*> clientsToRemove;
	for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
		Client* client = it->second;
		time_t now = time(NULL);
		if (now - client->getLastPingSent() > WAIT_FOR_PING_MAX) {
			clientsToRemove.push_back(client);
		}
	}

	for (size_t i = 0; i < clientsToRemove.size(); ++i) {
		Client* client = clientsToRemove[i];
		serverLog(client->getFd(), LOG_SERVER, C_ERR, "Ping timeout. Disconnecting client " + intToString(client->getFd()));
		removeClientFromServer(client);
	}
}

void IrcServer::run() {
	bool exitFlag = false;
	while (true) {
		try {
			checkPingTimeOut();

			int timeout_ms = 1000; // 1000ms = 1초
			if (poll(&_fds[0], _fds.size(), timeout_ms) < 0) {
				if (errno != EINTR) {
					exitFlag = true;
				}
				throw ServerException(ERR_POLL);
			}

			for (int i = _fds.size() - 1; i>= 0; --i) {
				// POLLIN event
				if (_fds[i].revents & POLLIN) {
					if (_fds[i].fd == this->_fd) {
						// Another(client) fd
						acceptClient();
					} else {
						// Another(client) fd
						handleSocketRead(_fds[i].fd); 

						// Add to remove list if Client Shutdown
						if (getClient(_fds[i].fd) == NULL) { 
							_fdsToRemove.push_back(_fds[i].fd);
						}
					}
				}

				// POLLOUT event
				if (_fds[i].revents & POLLOUT) {
					handleSocketWrite(_fds[i].fd);
					
					// Add to remove list if Client Shutdown
					if (getClient(_fds[i].fd) == NULL) { 
						_fdsToRemove.push_back(_fds[i].fd);
					}
				}
			}

			// Remove Client fds
			for (size_t j = 0; j < _fdsToRemove.size(); ++j) {
				removeClientFd(_fdsToRemove[j]);
			}
			_fdsToRemove.clear();

		} catch (const ServerException& e) {
			serverLog(this->_fd, LOG_ERR, C_ERR, e.what());
			if (exitFlag) {
				exit(EXIT_FAILURE);
			}
		}
	}
}

void IrcServer::handleSocketRead(int fd) {
	Client* client = getClient(fd);
	if (!client) {
		return;
	}
	char	buffer[BUFFER_SIZE];

	size_t recvLen = recv(fd, buffer, BUFFER_SIZE - 1, 0);
	
	// Recv error
	if (recvLen <= 0) {
		removeClientFromServer(getClient(fd));
		return ;
	}

	buffer[recvLen] = '\0';
	client->appendToRecvBuffer(buffer);

	std::string str;

	while (getClient(fd) && client->extractMessage(str)) {
		Cmd cmdHandler(*this, str, fd);
		serverLog(fd, LOG_INPUT, C_MSG, str);
		if (!cmdHandler.handleClientCmd())
			return ;
	}
}

void IrcServer::handleSocketWrite(int fd) {
	Client* client = getClient(fd);
	if (!client) {
		return;
	}

	std::string& sendBuffer = client->getSendBuffer();

	if (!sendBuffer.empty()) {
		ssize_t sendLen = send(fd, sendBuffer.c_str(), sendBuffer.size(), 0);
		if (sendLen > 0) {
			sendBuffer.erase(0, sendLen);
		} else if (sendLen == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
			removeClientFromServer(client);
		}
	}

	if (sendBuffer.empty()) {
		for (size_t i = 0; i < _fds.size(); ++i) {
			if (_fds[i].fd == fd) {
				_fds[i].events &= ~POLLOUT;
				break;
			}
		}
	}
}

void IrcServer::castMsg(int client_fd, const std::string msg) {
	Client* client = getClient(client_fd);
	if (!client) {
		return ;
	}

	std::string tosend = client->getSendBuffer() + msg;
	ssize_t sendLen = send(client_fd, tosend.c_str(), tosend.length(), 0);

	// Send error
	if (sendLen == -1) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			// Error: socket buffer is full
			client->appendToSendBuffer(msg);
			enablePollOutEvent(client_fd);
		} else if (errno == EPIPE || errno == ECONNRESET || errno == ENOTCONN) {
			// Error: another reasons
			removeClientFromServer(client);
		}

	// Only partial data was sent
	} else if (sendLen < static_cast<ssize_t>(msg.length())) {
		client->appendToSendBuffer(tosend.substr(sendLen));
		enablePollOutEvent(client_fd);
	// all Data Clear
	} else {
		client->clearSendBuffer(sendLen);
	}
	serverLog(client_fd, LOG_OUTPUT, C_MSG, msg.substr(0, msg.length()));
}

void IrcServer::broadcastMsg(const std::string& message, Channel* channel, int senderFd) {

	// with Channel, send a msg to all participants in that channel
	if (channel != NULL) {
		std::map<std::string, Client*>::iterator it = channel->getParticipant().begin();
		for (; it != channel->getParticipant().end(); ++it) {
			Client* receiver = it->second;
			if (senderFd != -1 && receiver->getFd() == senderFd)
				continue;
			castMsg(receiver->getFd(), message);
		}

	// without Channal, send a msg to all participants in this server
	} else {		
		std::map<int, Client*>::iterator it = this->_clients.begin();
		for (; it != _clients.end(); ++it) {
			int receiverFd = it->first;
			castMsg(receiverFd, message);
		}
	}
}

Client* IrcServer::getClient(int client_fd) {
	if (_clients.find(client_fd) == _clients.end())
		return NULL;

	return (_clients[client_fd]);
}

Client* IrcServer::getClient(const std::string& nickname) {
	for (std::map<int, Client*>::const_iterator it = _clients.begin(); it != _clients.end(); ++it) {
		if (it->second->getNickname() == nickname) {
			return it->second;
		}
	}
	return NULL;
}

std::map<std::string, Channel*>& IrcServer::getChannels()
{
	return (this->_channels);
}

void IrcServer::removeChannel(const std::string channelName) {
	std::map<std::string, Channel*>::iterator it = _channels.find(channelName);
	if (it != _channels.end()) {
		delete it->second;  // 채널 객체 먼저 할당 해제
		_channels.erase(it);  // 채널 삭제 (참조 못하게 아예 채널 목록에서 삭제)
	}
}

const std::map<std::string, Client*>& IrcServer::getNickNameClientMap() const
{
	return (this->nickNameClientMap);
}

void IrcServer::addClientByNickname(const std::string& nickname, Client* client)
{
	this->nickNameClientMap[nickname] = client;
}

void IrcServer::setChannels(const std::string& channelName, const std::string& key, const char& mode) {
	_channels[channelName] = new Channel(channelName);
	if (key != "")
		_channels[channelName]->setKey(key);
	if (mode != '\0')
		_channels[channelName]->setMode(mode);
}

void IrcServer::printGoat() {
	std::ifstream goatFile(PATH_GOAT);
	if (goatFile.is_open()) {
		std::string line;
		while (getline(goatFile, line)) {
			std::cout << line << std::endl;
		}
		goatFile.close();
	} else {
		serverLog(this->_fd, LOG_ERR, C_ERR, ERR_OPEN_FILE);
	}
}

void IrcServer::removeClientFromServer(Client* client) {
	if (client == NULL) {
		return;
	}

	for (std::map<std::string, Channel*>::iterator chs = _channels.begin(); chs != _channels.end(); ++chs) {
		Channel* ch = chs->second;
		if (ch->isParticipant(ch->isOperatorNickname(client->getNickname()))) {
			ch->removeParticipant(ch->isOperatorNickname(client->getNickname()));
		}
		if (ch->isOperator(client->getNickname())) {
			ch->removeOperator(client->getNickname());
		}
	}

	// 클라이언트를 _clients와 nickNameClientMap에서 제거
	if (_clients.find(client->getFd()) != _clients.end()) {
		_clients.erase(client->getFd());
	}
	nickNameClientMap.erase(client->getNickname());

	_fdsToRemove.push_back(client->getFd());
	
	serverLog(this->_fd, LOG_SERVER, C_ERR, MSG_END_CONNECTED(intToString(client->getFd())));
	delete client;
}

const char* IrcServer::ServerException::what() const throw() {
	return msg.c_str();
}

std::string IrcServer::makeMsg(const std::string& prefix, const std::string& msg) {
	return (prefix + " " + msg + CRLF);
}

void IrcServer::updateClients(Client* client) {
	_clients.erase(client->getFd());
	_clients[client->getFd()] = client;
}

void IrcServer::updateNickNameClientMap(const std::string& oldNick, const std::string& newNick, Client* client) {
	nickNameClientMap.erase(oldNick);
	nickNameClientMap[newNick] = client;
}

void IrcServer::serverLog(int fd, int log_type, std::string log_color, std::string msg) {
    if (msg.empty() || msg[msg.size() - 1] != '\n') {
        msg += '\n';
    }

    if (log_type == LOG_OUTPUT) {
        std::cout << "OUTPUT[" << C_KEY << fd << C_RESET << "]: " << log_color << msg << C_RESET;
    } else if (log_type == LOG_INPUT) {
        std::cout << "INPUT[" << C_KEY << fd << C_RESET << "]: " << log_color << msg << C_RESET;
    } else if (log_type == LOG_SERVER) {
        std::cout << "SERV_LOG: " << log_color << msg << C_RESET;
    } else if (log_type == LOG_ERR) {
        std::cerr << "SERV_LOG: " << log_color << msg << C_RESET;
    }
}


std::string IrcServer::intToString(int num) {
	std::stringstream ss;
	ss << num;
	return ss.str();
}

void IrcServer::enablePollOutEvent(int client_fd) {
	for (size_t i = 0; i < _fds.size(); ++i) {
		if (_fds[i].fd == client_fd) {
			_fds[i].events = (_fds[i].events | POLLOUT);
			break;
		}
	}
}

void IrcServer::removeClientFd(int client_fd) {
	for (size_t i = 0; i < _fds.size(); ++i) {
		if (_fds[i].fd == client_fd) {
			_fds.erase(_fds.begin() + i);
			break;
		}
	}
}