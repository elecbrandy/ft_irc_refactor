#include "Cmd.hpp"

/*
	NICK

	1. 닉네임을 설정하거나 기존의 닉네임을 변경하는 명령어
	2. 중복 불가
	3. 닉네임은 영문 문자만 가능하며 최대 9자로 구성
	(영문 문자나 특수문자로 시작하며 최대 9자로 구성)

	Numeric Replies
	ERR_NONICKNAMEGIVEN: 닉네임이 제공되지 않았을 때 발생
	ERR_NICKNAMEINUSE: 사용하려는 닉네임이 이미 다른 사용자가 사용 중일 때 발생
	ERR_ERRONEUSNICKNAME: 잘못된 형식의 닉네임이 제공되었을 때 발생
	? - ERR_UNAVAILRESOURCE: 사용할 수 없는 자원이거나 닉네임이 제한되어 있을 때 발생
	? - ERR_RESTRICTED: 사용자가 닉네임 변경 권한이 제한되어 있을 때 발생

	Sat Nov 09 2024 08:49:52 USERINPUT: C[811AAAAAH] I NICK 1234
	Sat Nov 09 2024 08:49:52 USEROUTPUT: C[811AAAAAH] O :irc.local 432 ranchoi 1234 :Erroneous Nickname

	Sat Nov 09 2024 08:50:32 USERINPUT: C[811AAAAAH] I NICK
	Sat Nov 09 2024 08:50:32 USEROUTPUT: C[811AAAAAH] O :irc.local 461 ranchoi NICK :Not enough parameters

	Sat Nov 09 2024 08:50:46 USERINPUT: C[811AAAAAH] I NICK
	Sat Nov 09 2024 08:50:46 USEROUTPUT: C[811AAAAAH] O :irc.local 461 ranchoi NICK :Not enough parameters.

	Sat Nov 09 2024 08:50:58 USERINPUT: C[811AAAAAH] I NICK a
	Sat Nov 09 2024 08:50:58 USEROUTPUT: C[811AAAAAH] O :ranchoi!root@127.0.0.1 NICK :a
*/

/* ranchoi
닉네임 변경 시
0. 변경 전 닉네임을 저장
1. 기존 클라이언트의 닉네임 / 프리픽스 변경
2. server->_clients, server->nickNameClientMap 업데이트
3. 기존 클라이언트가 참여하고 있는 채널 참여자 목록 업데이트
	3.1. 채널 참여자 목록 업데이트
	3.2. 채널 운영자 목록 업데이트 (운영자인 경우)
4. 닉네임 변경 메세지 전송
*/

void Cmd::checkNick(const std::string& str) {
	/* CLIENT NICK check */	
	std::string tmp = client->getNickname();
	if (tmp.empty()) {
		tmp = "guest";
	}

	/* EMPTY check */
	if (str.empty())
		throw CmdException(ERR_NONICKNAMEGIVEN(tmp));

	/* SIZE check */
	if (str.size() > NICK_MAX_LEN)
		throw CmdException(server.makeMsg(PREFIX_SERVER, ERR_ERRONEUSNICKNAME(tmp)));

	/* ALNUM check */
	for (std::string::const_iterator it = str.begin(); it != str.end(); ++it) {
		if (!std::isalnum(static_cast<unsigned char>(*it)))
			throw CmdException(server.makeMsg(PREFIX_SERVER, ERR_ERRONEUSNICKNAME(tmp)));
	}

	/* INUSE check */
	if (server.getClient(str) != NULL)
		throw CmdException(server.makeMsg(PREFIX_SERVER, ERR_NICKNAMEINUSE(tmp)));
}

// std::map<std::string, Client*>		nickNameClientMap;

void Cmd::cmdNick() {
	// nick 명령어를 처리하기 전에 PASS 명령어를 먼저 처리했어야 정상적인 작동!
	if (client->getPassStatus() == false)
		throw CmdException(server.makeMsg(PREFIX_SERVER, ERR_NOTREGISTERED(client->getNickname())));
		
	checkNick(this->cmdParams);

	// 0. 변경 전 닉네임을 저장
	std::string newNick = this->cmdParams;
	std::string oldNick = client->getNickname();
	std::string oldPrefix = client->getPrefix();

	// 1. 클라이언트의 닉네임 적용/변경
	client->setNickname(newNick);

	// 기존 클라이언트일때만 실행
	if (client->getRegisteredStatus() == true) {
		// 기존의 프리픽스 변경
		client->setPrefix();

		// 2. server->_clients, server->nickNameClientMap 업데이트
		server.updateClients(client);
		server.updateNickNameClientMap(oldNick, newNick, client);

		// 3. channel->participant, channel->operator 업데이트
		std::map<std::string, Channel *>::iterator it;
		for (it = server.getChannels().begin(); it != server.getChannels().end(); ++it) {
			Channel* ch = it->second;
			if (ch->isParticipant(ch->isOperatorNickname(oldNick))) {
				if (ch->isOperator(oldNick)) {
					ch->removeParticipant(ch->isOperatorNickname(oldNick));
					ch->addParticipant("@" + newNick, client);

					ch->removeOperator(oldNick);
					ch->addOperator(newNick, client);
				} else {
					ch->removeParticipant(oldNick);
					ch->addParticipant(newNick, client);
				}
			}
		}
		server.broadcastMsg(server.makeMsg(":" + oldNick, RPL_NICK(newNick)), NULL, -1);
	} else
		this->server.addClientByNickname(this->cmdParams, this->client);
	client->setNickStatus(true);
}