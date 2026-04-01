#include "Cmd.hpp"

/*
	PASS
	1. 닉네임을 설정하거나 기존의 닉네임을 변경하는 명령어
	2. 이미 사용 중인 닉네임인 경우 에러 응답 ERR_NICKCOLLISION
	3. 닉네임은 영문 문자나 특수문자로 시작하며 최대 9자로 구성

	Numeric 응답 코드
	ERR_NEEDMOREPARAMS: 비밀번호가 제공되지 않았을 때 발생
	ERR_ALREADYREGISTRED: 이미 서버에 등록된 상태에서 PASS 명령어를 다시 보낼 때 발생
	ERR_PASSWDMISMATCH: 첫 PASS 시도에서 비번이 틀렸을 경우.
*/

void Cmd::cmdPass() {
	// 비밀번호 파라미터가 없는 경우
	if (cmdParams.empty())
		throw CmdException(server.makeMsg(PREFIX_SERVER, ERR_NEEDMOREPARAMS(client->getNickname(), "PASS")));

	// 이미 등록된 상태에서 PASS 명령어를 다시 보낸 경우
	if (client->getPassStatus())
		throw CmdException(server.makeMsg(PREFIX_SERVER, ERR_ALREADYREGISTRED(client->getNickname())));

	// 비밀번호가 틀린 경우
	if (this->cmdParams != server.getPassword())
		throw CmdException(server.makeMsg(PREFIX_SERVER, ERR_PASSWDMISMATCH(client->getNickname())));
	
	client->setPassStatus(true);
}
