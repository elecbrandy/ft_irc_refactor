#include "Channel.hpp"

Channel::Channel(std::string name) :
_name(name), _key(""), _topic(""), _mode() , _participant() {}

Channel::~Channel() {}

/* setter */
void Channel::setKey(std::string key) {_key = key;}

void Channel::setTopic(std::string topic) {_topic = topic;}

void Channel::setMode(char mode) {_mode.insert(mode);}

void Channel::addParticipant(std::string participantName, Client* client) {
    _participant.insert(std::make_pair(participantName, client));
}

void Channel::setLimit(unsigned int limit) {_limit = limit;}

void Channel::addInvited(std::string nickname) {_invited.push_back(nickname);}

void Channel::addOperator(std::string nick, Client* client)
{
	this->_operator[nick] = client;
}

void Channel::removeOperator(std::string nickname)
{
	this->_operator.erase(nickname);
}

void Channel::removeKey()
{
	this->_key = "";
}

void Channel::removeMode(char mode)
{
	this->_mode.erase(mode);
}

const std::string Channel::isOperatorNickname(std::string nickname) const
{
	if (isOperator(nickname) == true)
		return "@" + nickname;
	else
		return nickname;
}

void Channel::removeParticipant(std::string target){
	_participant.erase(target);
}

/* getter */
std::string Channel::getName() {return this->_name;}

std::string Channel::getKey() {return this->_key;}

std::string Channel::getTopic() {return this->_topic;}

const std::set<char>& Channel::getMode() const {return this->_mode;}

std::map<std::string, Client*>& Channel::getParticipant() {return this->_participant;}

std::string Channel::getParticipantNameStr() {
    std::string names = "";
    std::map<std::string, Client*>::iterator it = _participant.begin();
    while (it != _participant.end()) {
        // Client 객체에서 순수한 닉네임만 가져오기
        std::string nickname = it->second->getNickname();
        // 운영자(@) 표시가 필요한 경우 보존
        if (it->first[0] == '@') {
            names += '@';
        }
        names += nickname;
        ++it;
        if (it != _participant.end()) {
            names += " ";
        }
    }
    return names;
}

#include <iostream>
bool Channel::isOperator(std::string nickname) const
{	
	if (this->_operator.find(nickname) == this->_operator.end())
		return false;
	else
		return true;
}

bool Channel::isParticipant(std::string nickname) const
{
	if (this->_participant.find(nickname) == this->_participant.end())
		return false;
	else
		return true;
}

bool Channel::isSetKey() const
{
	if (this->_key == "")
		return (false);
	else
		return true;
}

void Channel::removeInvited(std::string nickname) {
	_invited.erase(std::remove(_invited.begin(), _invited.end(), nickname), _invited.end());
}

std::map<std::string, Client*>& Channel::getOperator() {return this->_operator;}

unsigned int Channel::getLimit() const {return this->_limit;} 

std::vector<std::string> Channel::getInvited() {return this->_invited;}