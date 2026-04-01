#include "Cmd.hpp"

/*
	USER
	- 명령어 : USER
	- 매개변수 : <user> <mode> <unused> <realname>

	Numeric 응답 코드
	- ERR_NEEDMOREPARAMS: 필요한 매개변수가 부족한 경우 반환
	- ERR_ALREADYREGISTRED: 클라이언트가 이미 등록된 상태에서 USER 명령을 다시 보낼 때 반환

	Command: USER
	Parameters: <username> <hostname> <servername> <realname>
	USER 메시지는 새로운 사용자가 연결될 때 사용자의 이름, 호스트 이름, 서버 이름, 실제 이름을 지정하기 위해 처음에 사용됩니다.
	또한, 서버 간 통신에서 새로운 사용자가 IRC에 접속했음을 알리기 위해 사용됩니다.
	클라이언트로부터 USER와 NICK 두 메시지를 모두 받은 후에야 사용자가 등록됩니다.
	
	서버 간 통신에서 USER는 클라이언트의 NICKname으로 접두어가 붙어야 합니다.
	USER 명령어가 직접 연결된 클라이언트로부터 온 경우에는 (보안상의 이유로) IRC 서버에서 호스트 이름과 서버 이름을 무시하지만,
	서버 간 통신에서는 사용됩니다. 이는 새로운 사용자가 네트워크의 다른 서버에 소개될 때, 항상 NICK이 USER와 함께 원격 서버에 전송되어야 한다는 것을 의미합니다.

	또한, realname 매개변수는 마지막 매개변수여야 하며, 공백 문자가 포함될 수 있기 때문에
	반드시 콜론(':')으로 접두어가 붙어야 해당 항목으로 인식됩니다.
	USER 메시지만을 이용하여 클라이언트가 자신의 사용자 이름을 속이기 쉬운 관계로,
	"Identity Server"(인증 서버)를 사용하는 것이 권장됩니다.
	사용자가 연결된 호스트에 인증 서버가 활성화되어 있다면, USER 메시지의 사용자 이름은 인증 서버의 응답에 따라 설정됩니다.

	숫자 응답: ERR_NEEDMOREPARAMS              ERR_ALREADYREGISTRED
	
	예시: USER guest tolmoon tolsun :Ronnie Reagan;
	"guest"라는 사용자 이름과 "Ronnie Reagan"이라는 실제 이름으로 등록.
	
	:testnick USER guest tolmoon tolsun :Ronnie Reaga;
	서버 간 메시지에서 USER 명령어가 속한 사용자 이름 "testnick"과 함께.
*/


void Cmd::checkUsername(const std::string& str) {
	/* EMPTY check */
	if (str.empty()) {
		if (this->client->getNickname().empty()) {
			throw CmdException(server.makeMsg(PREFIX_SERVER, ERR_ERRUSERCMD));
		}
	}

	/* SIZE check */
	if (str.size() > USERNAME_MAX_LEN) {
		throw CmdException(server.makeMsg(PREFIX_SERVER, ERR_ERRUSERCMD));
	}
	
	/* ALNUM check */
	for (std::string::const_iterator it = str.begin(); it != str.end(); ++it) {
		if (!std::isalnum(static_cast<unsigned char>(*it))) {
			throw CmdException(server.makeMsg(PREFIX_SERVER, ERR_ERRUSERCMD));
		}
	}
}

void Cmd::checkRealname(const std::string& str) {
	/* EMPTY check */
	if (str.empty()) {
		if (this->client->getNickname().empty()) {
			throw CmdException(server.makeMsg(PREFIX_SERVER, ERR_ERRUSERCMD));
		}
	}

	/* SIZE check */
	if (str.size() > REALNAME_MAX_LEN) {
		throw CmdException(server.makeMsg(PREFIX_SERVER, ERR_ERRUSERCMD));
	}

	/* ALPHA or SPACE check */
	for (std::string::const_iterator it = str.begin(); it != str.end(); ++it) {
		if (!(std::isalpha(static_cast<unsigned char>(*it)) || std::isspace(static_cast<unsigned char>(*it)))) {
			throw CmdException(server.makeMsg(PREFIX_SERVER, ERR_ERRUSERCMD));
		}
	}
}

void Cmd::cmdUser() {
	// 이미 등록된 상태인 경우
	if (this->client->getRegisteredStatus()) {
		throw CmdException(server.makeMsg(PREFIX_SERVER, ERR_ALREADYREGISTRED(client->getNickname())));
	}

	// PASS 명령어가 먼저 수신되지 않은 경우
	if (!this->client->getPassStatus()) {
		throw CmdException(server.makeMsg(PREFIX_SERVER, ERR_NEEDMOREPARAMS(client->getNickname(), "PASS")));
	}

	std::stringstream ssArg(cmdParams);
	std::string token;
	int count = 0;

	/* 매개변수 개수 체크 */
	while (ssArg >> token) {
		++count;
	}
	if (count < 4) {
		throw CmdException(server.makeMsg(PREFIX_SERVER, ERR_NEEDMOREPARAMS(client->getNickname(), "USER")));
	}

	std::stringstream ssUser(cmdParams);
	count = 0;
	while (ssUser >> token) {
		if (token.empty()) {
			throw CmdException(server.makeMsg(PREFIX_SERVER, ERR_NEEDMOREPARAMS(client->getNickname(), "USER")));
		}
		switch (count) {
		case 0:
			/* Username */
			checkUsername(token);
			client->setUsername(token);
			break;
		case 1: 
			/* Hostname : set in Client() */
			break;
		case 2: 
			/* Servername */
			client->setServername(server.getName());
			break;
		case 3: 
			/* Realname (starts with ':') */
			if (token[0] == ':') {
				// 첫 번째 토큰의 ':' 이후 부분을 realname에 추가
				std::string realname = token.substr(1);

				// 이후 남은 모든 토큰을 realname에 공백을 포함해 붙여넣음
				while (ssUser >> token) {
					realname += " " + token;
				}

				// Realname에 대한 유효성 검사
				checkRealname(realname);
				client->setRealname(realname);
			} else {
				throw CmdException(server.makeMsg(PREFIX_SERVER, ERR_NEEDMOREPARAMS(client->getNickname(), "USER")));
			}
			break;
		}
		++count;
	}
	client->setPrefix();
	client->setUserStatus(true);

	if (client->getPassStatus() && client->getNickStatus() && client->getUserStatus()) {
		client->setRegisteredStatus(true);
		printWelcome();
	}
}
