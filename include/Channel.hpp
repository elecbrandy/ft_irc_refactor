#ifndef CHANNEL_HPP
# define CHANNEL_HPP

# include <algorithm>
# include <set>
# include <vector>
# include <map>
# include "Client.hpp"

# define INVITE_MODE 'i'
# define TOPIC_MODE 't'
# define KEY_MODE 'k'
# define OPERATOR_MODE 'o'
# define LIMIT_MODE 'l'
# define DEFAULT_LIMIT 10

class Channel {
	private:
		std::string				_name; //채널명
		std::string				_key; //채널 비밀번호
		std::string				_topic; //채널 주제
		std::set<char>			_mode; //채널 모드
		std::map<std::string, Client*> 		_participant; //채널 참여자
		std::map<std::string, Client*>		_operator; //채널 운영자  //set으로 하면 중복 안되고 자동 정렬이 유용함

		unsigned int						_limit; //채널 참여자 제한 수
		std::vector<std::string> _invited; //초대된 사용자 목록

	public:
		Channel(std::string name);
		~Channel();

		void setKey(std::string key);
		void setTopic(std::string topic);
		void setMode(char mode);
		void setLimit(unsigned int limit);

		void addInvited(std::string nickname);
		void addOperator(std::string nick, Client* client);
		void addParticipant(std::string participantName,Client* client);
		void addParticipantname(std::string participantName);

		void removeKey();
		void removeOperator(std::string nickname);
		void removeInvited(std::string nickname);
		void removeMode(char mode);
		void removeParticipant(std::string target);

		const std::string isOperatorNickname(std::string nickanme) const;

		std::string getName();
		std::string getKey();
		std::string getTopic();
		const std::set<char>& getMode() const;
		std::map<std::string, Client*>& getParticipant();
		std::map<std::string, Client*>& getOperator();
		std::string getParticipantNameStr();

		bool isOperator(std::string nickname) const;
		bool isParticipant(std::string nickname) const;
		bool isSetKey() const;
		unsigned int getLimit() const;
		std::vector<std::string> getInvited();
		std::vector<std::string> getBanned();

};


#endif