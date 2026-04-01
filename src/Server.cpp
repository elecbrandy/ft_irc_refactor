#include "../include/Server.hpp"
#include "../include/Client.hpp"

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
    while (!shouldExitServer()) {
        try {
            checkPingTimeOut();
 
            int timeout_ms = 1000;
            if (poll(&_fds[0], _fds.size(), timeout_ms) < 0) {
                if (errno == EINTR) {
                    continue;  // 신호로 인한 중단은 무시하고 계속
                }
                // 다른 에러는 catch 블록에서 처리
                throw ServerException(ERR_POLL);
            }
 
            for (size_t i = 0; i < _fds.size(); ++i) {
                if (_fds[i].revents == 0) {
                    continue;  // 이벤트 없으면 스킵
                }
 
                // 소켓 이벤트 처리
                if (_fds[i].fd == this->_fd) {
                    if (!processServerEvent()) {
                        throw ServerException(ERR_ACCEPT_CLIENT);
                    }
                    continue;
                }
 
                int client_fd = _fds[i].fd;
 
                // 클라이언트 읽기 처리
                if (_fds[i].revents & POLLIN) {
                    if (!processClientRead(client_fd)) {
                        // 에러 발생, 해당 클라이언트 제거 표시
                        markClientForRemoval(client_fd);
                        continue;
                    }
                }
 
                // 쓰기 처리 (존재 여부만 간단히 확인)
                // _fds[i].fd는 이미 -1로 변경되었을 수 있으므로 재검사 필요
                int current_fd = _fds[i].fd;
                if (current_fd != -1 && (_fds[i].revents & POLLOUT)) {
                    if (!processClientWrite(current_fd)) {
                        markClientForRemoval(current_fd);
                    }
                }
            }
 
            // 중앙화된 cleanup (루프 끝에서 한 번만)
            cleanupMarkedClients();
 
        } catch (const ServerException& e) {
            serverLog(this->_fd, LOG_ERR, C_ERR, e.what());
            // 복구 가능한 에러는 계속, 아니면 종료
            if (!canRecover(e)) {
                break;
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
		return;
	}

	buffer[recvLen] = '\0';
	client->appendToRecvBuffer(buffer);
	
	std::string str;
	while (getClient(fd) && client->extractMessage(str)) {
		Cmd cmdHandler(*this, str, fd);
		serverLog(fd, LOG_INPUT, C_MSG, str);
		if (!cmdHandler.handleClientCmd())
			return;
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

	// 1. 참여 중인 채널에서 퇴장 처리 (기존과 동일)
	for (std::map<std::string, Channel*>::iterator chs = _channels.begin(); chs != _channels.end(); ++chs) {
		Channel* ch = chs->second;
		if (ch->isParticipant(ch->isOperatorNickname(client->getNickname()))) {
			ch->removeParticipant(ch->isOperatorNickname(client->getNickname()));
		}
		if (ch->isOperator(client->getNickname())) {
			ch->removeOperator(client->getNickname());
		}
	}

	// 2. Map에서 클라이언트 제거
	if (_clients.find(client->getFd()) != _clients.end()) {
		_clients.erase(client->getFd());
	}
	nickNameClientMap.erase(client->getNickname());

	// 3. pollfd 배열에서 해당 소켓을 무효화(-1로 설정)
	for (size_t i = 0; i < _fds.size(); ++i) {
		if (_fds[i].fd == client->getFd()) {
			_fds[i].fd = -1; // 무효화
			break;
		}
	}
	
	// 4. 로그 출력 및 자원 해제
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

/* 
	processServerEvent()
	- 서버 소켓 이벤트 처리
	- 새로운 클라이언트 연결 수락
   	- 기존 acceptClient()를 래핑하되, 에러 처리 개선
   
	return:
	- true: 정상 처리 (또는 무시 가능한 에러)
	- false: 복구 불가능한 에러
*/
bool IrcServer::processServerEvent() {
    try {
        acceptClient();
        return true;
    } catch (const ServerException& e) {
        return false;
    }
}

/* 
	processClientRead(int fd)
	- 클라이언트 읽기 처리
	- 기존 handleSocketRead() 로직을 유지
	- bool 반환 추가 (실패 시 false)
	- 클라이언트 삭제 시 명시적 신호
   
	return:
	- true: 정상 처리 (또는 무시 가능한 에러)
	- false: 클라이언트 삭제 필요
*/
bool IrcServer::processClientRead(int fd) {
    Client* client = getClient(fd);
    if (!client) {
        return false;  // 클라이언트가 없음 (이미 삭제됨)
    }

    char buffer[BUFFER_SIZE];
    size_t recvLen = recv(fd, buffer, BUFFER_SIZE - 1, 0);
    
    // 연결 종료 또는 에러
    if (recvLen <= 0) {
		return false;  // 클라이언트 제거 필요
	}

    buffer[recvLen] = '\0';
    client->appendToRecvBuffer(buffer);
    
    // 메시지 추출 및 처리
    std::string str;
    while (getClient(fd) && client->extractMessage(str)) {
		try {
			Cmd cmdHandler(*this, str, fd);
			serverLog(fd, LOG_INPUT, C_MSG, str);
			
			// handleClientCmd()가 false 반환 시 클라이언트가 삭제된 것이므로 루프 탈출
			if (!cmdHandler.handleClientCmd()) {
				return false;  // 클라이언트가 삭제됨
			}
		} catch (std::exception& e) {
			serverLog(fd, LOG_ERR, C_ERR, std::string("Error processing command: ") + e.what());
		}
    }
    
    return true;  // 정상 처리
}

/*
	processClientWrite(int fd)
	- 클라이언트 쓰기 처리
	- 기존 handleSocketWrite() 로직 유지
	- bool 반환 추가
	- 에러 처리 명확화 
*/ 
bool IrcServer::processClientWrite(int fd) {
    Client* client = getClient(fd);
    if (!client) {
        return false;
    }

    std::string& sendBuffer = client->getSendBuffer();

    if (!sendBuffer.empty()) {
        ssize_t sendLen = send(fd, sendBuffer.c_str(), sendBuffer.size(), 0);
        
        if (sendLen > 0) {
            // 부분 또는 전체 전송 성공
            sendBuffer.erase(0, sendLen);
        } else if (sendLen == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
            // 복구 불가능한 에러
            removeClientFromServer(client);
            return false;
        }
        // EAGAIN/EWOULDBLOCK은 무시하고 나중에 재시도
    }

    // 전송 완료 시 POLLOUT 이벤트 비활성화
    if (sendBuffer.empty()) {
        for (size_t i = 0; i < _fds.size(); ++i) {
            if (_fds[i].fd == fd) {
                _fds[i].events &= ~POLLOUT;
                break;
            }
        }
    }

    return true;  // 정상 처리
}

/* 
	markClientForRemoval(int fd)
	- 클라이언트 제거 표시
	- fd를 제거 대상으로 표시 (즉시 삭제하지 않음)
   	- cleanup 단계에서 일괄 처리
		- 루프 중간에 _clients 수정 안 함
   		- 인덱스 변경 없음
   		- 타이밍 이슈 방지
*/
void IrcServer::markClientForRemoval(int fd) {
	Client* client = getClient(fd);
	if (!client)
		return ;

	serverLog(fd, LOG_SERVER, C_WARN, "Marking client for removal: " + intToString(fd));

    // _fds에서 해당 fd의 항목을 찾아 -1로 표시
    for (size_t i = 0; i < _fds.size(); ++i) {
        if (_fds[i].fd == fd) {
            _fds[i].fd = -1;  // 마킹
            break;
        }
    }
    removeClientFromServer(getClient(fd));  // 클라이언트 제거 처리 (채널 퇴장 등)
}


/* ========================================
   5. cleanupMarkedClients() - 제거 대상 정리
   
   목적:
   - markClientForRemoval()로 표시된 fd 정리
   - _clients와 _fds 동시 정리
   
   호출 시점:
   - run() 루프의 폴 순회 완료 후
   - 모든 fd 접근이 완료된 후에만 호출
   ======================================== */

void IrcServer::cleanupMarkedClients() {
    // 1. _fds에서 -1인 항목 제거
    std::vector<struct pollfd>::iterator it = _fds.begin();

    while (it != _fds.end()) {
        if (it->fd == -1) {
            it = _fds.erase(it);  // 제거 후 다음 항목으로 이동
        } else {
            ++it;
        }
    }
}

/*
	shouldExitServer()
	- 서버 종료 판단
	- 명확한 종료 조건 제공
	- run() while 루프 조건으로 사용
	- exitFlag 내부 상태 체크
   - 신호 처리 등 확장 가능
*/
bool IrcServer::shouldExitServer() {
    // 현재는 단순하지만, 나중에 확장 가능
    // 예: signal handler가 설정한 플래그 체크
    return false;  // 무한 루프 (신호에 의해서만 종료)
}



/*
	canRecover(const ServerException& e)
	- 서버 종료 판단
	- catch 블록에서 에러 타입에 따라 계속할지 말지 판단하는 데 사용
*/
bool IrcServer::canRecover(const ServerException& e) {
    std::string msg = e.what();
    
    // 복구 불가능한 에러
    if (msg.find("socket") != std::string::npos ||
        msg.find("bind") != std::string::npos ||
        msg.find("listen") != std::string::npos) {
        return false;  // 서버 자체의 문제 → 종료
    }
    
    // 복구 가능한 에러 (클라이언트 관련)
    // accept, read, write 에러는 무시하고 계속
    return true;
}