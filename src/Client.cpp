#include "Client.hpp"
#include "Server.hpp"

Client::Client(in_addr addr)
:	_hostname(inet_ntoa(addr)),
	_lastPingSent(time(NULL)) {
	_registerStatus.pass = false;
	_registerStatus.nick = false;
	_registerStatus.user = false;
	_registerStatus.registered = false;
}

Client:: ~Client() {
	close(this->_fd);
}

/* setter */

void Client::setFd(int fd) {this->_fd = fd;}

void Client::setNickname(const std::string& str) {
	size_t spacePos = str.find(' ');
	if (spacePos != std::string::npos) {
		this->_nickname = str.substr(0, spacePos);
	} else {
		this->_nickname = str;
	}
}

void Client::setUsername(const std::string& str) {this->_username = str;}

void Client::setHostname(const std::string& str) {this->_hostname = str;}

void Client::setRealname(const std::string& str) {this->_realname = str;}

void Client::setServername(const std::string& str) {this->_servername = str;}

void Client::updateLastPingSent() {this->_lastPingSent = time(NULL);}

void Client::setPassStatus(bool status) {this->_registerStatus.pass = status;}

void Client::setNickStatus(bool status) {this->_registerStatus.nick = status;}

void Client::setUserStatus(bool status) {this->_registerStatus.user = status;}

void Client::setRegisteredStatus(bool status) {this->_registerStatus.registered = status;}


/* getter */
int	Client::getFd() const {return this->_fd;}

std::string	Client::getNickname() const {return this->_nickname;}

std::string	Client::getUsername() const {return this->_username;}

std::string	Client::getHostname() const {return this->_hostname;}

std::string	Client::getRealname() const {return this->_realname;}

std::string	Client::getServername() const {return this->_servername;}

time_t	Client::getLastPingSent() const {return this->_lastPingSent;}

bool Client::getPassStatus() const {return this->_registerStatus.pass;}

bool Client::getNickStatus() const {return this->_registerStatus.nick;}

bool Client::getUserStatus() const {return this->_registerStatus.user;}

bool Client::getRegisteredStatus() const {return this->_registerStatus.registered;}

std::string Client::getPrefix() const {return this->_prefix;}

// time_t Client::getLastPongSent() const {return this->_lastPongSent;}

/* other */
void Client::appendToRecvBuffer(const std::string& str) {
	this->_recvBuffer += str;
}

// Ver1: 512~ delete
// bool Client::extractMessage(std::string& message) {
//     size_t pos = _recvBuffer.find(CRLF);

// 	/* with CRLF */
//     if (pos != std::string::npos) {
//         size_t messageLen = pos + 2;
//         if (messageLen > BUFFER_SIZE - 1) {
//             _recvBuffer.erase(0, messageLen);
//             return false;
//         } else {
//             message = _recvBuffer.substr(0, pos);
//             _recvBuffer.erase(0, messageLen);
//             return true;
//         }
// 	/* without CRLF */
//     } else if (_recvBuffer.length() >= BUFFER_SIZE - 1) {
//         _recvBuffer.clear();
//         return false;
//     }
//     return false;
// }

// Ver2: 512~ extract
bool Client::extractMessage(std::string& message) {
	size_t pos = _recvBuffer.find("\r\n");
	if (pos != std::string::npos) {
		size_t messageLength = pos + 2;
		
		// Check len 
		if (messageLength > 512) {
			// 앞의 510자와 CRLF를 분리
			message = _recvBuffer.substr(0, 510);
			_recvBuffer.erase(0, 510); // 앞의 510자를 제거하고 남은 부분은 다음 처리 대상
		} else {
			// 정상적인 메시지 처리
			message = _recvBuffer.substr(0, pos);
			_recvBuffer.erase(0, pos + 2);
		}
		return true;
	} else if (_recvBuffer.length() >= 512) {
		// CRLF가 없지만 버퍼가 512자를 초과한 경우
		message = _recvBuffer.substr(0, 510);
		_recvBuffer.erase(0, 510);
		return true;
	}
	return false;
}

void Client::appendToSendBuffer(const std::string& data) {
	this->_sendBuffer += data;
}

std::string& Client::getSendBuffer() {
	return this->_sendBuffer;
}

void Client::clearSendBuffer(size_t count) {
	this->_sendBuffer.erase(0, count);
}

bool Client::hasDataToSend() const {
	if (this->_sendBuffer.empty()) {
		return false;
	}
	return true;
}

void Client::setPrefix() {_prefix = PREFIX_USER(_nickname, _username, _hostname);}

void Client::printLog() {
	std::cout << C_WARN << std::boolalpha
	<< ">>>>> client log <<<<<\n"
	<< "nickname: " << this->getNickname() << "\n"
	<< "username: " << this->getUsername() << "\n"
	<< "hostname: " << this->getHostname() << "\n"
	<< "realname: " << this->getRealname() << "\n"
	// << "servername: " << this->getServername() << "\n"
	<< "registerStatus: " << this->getRegisteredStatus() << "\n"
	<< C_RESET << std::endl;
}
