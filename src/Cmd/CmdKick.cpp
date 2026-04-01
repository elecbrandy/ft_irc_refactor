#include "Cmd.hpp"

// /*
// 명령어: KICK <channel> <nickname> [<comment>]

// Numeric Replies:
// ERR_NEEDMOREPARAMS - 파라미터 부족
// ERR_BADCHANMASK - 주어진 채널이름의 형식이 잘못됨. (채널이름이 규칙에 맞지 않는 경우, #으로 시작하지 않는 등..)
// ERR_NOSUCHCHANNEL - 채널이 채널 목록에 존재하지 않음
// ERR_NOTONCHANNEL - 강퇴시킬 사용자가 채널에 참여자가 아님
// ERR_CHANOPRIVSNEEDED - 채널 오퍼레이터 권한이 없음

// - 채널에서 사용자를 강제로 퇴장시킬 때 사용하는 명령어 (강제 part)
// - <channel>은 사용자를 강제로 퇴장시킬 채널 이름
// - <nickname>은 강제로 퇴장시킬 사용자의 닉네임
// - <comment>는 선택사항으로, 사용자를 강제로 퇴장시킬 때 이유를 명시

// 경우의 수
// 1. 파라미터 부족/과다 : 최소 채널이름과 닉네임이 필요 (총 2개, 최대 3개)
// 2. 채널 파라미터 잘못된 경우
// 	2.1 채널이름의 형식이 잘못된 경우
// 	2.2 채널이 존재하지 않는 경우
// 3. 강퇴시킬 사용자가 채널의 참여자가 아닌 경우
// 4. 채널 오퍼레이터 권한이 없는 경우
// */

std::vector<std::string> Cmd::split(char delim) {
    std::vector<std::string> res;
    std::string paramCopy = getCmdParams();
    size_t start = 0, pos;

    // ':'가 나올 때까지 delim을 기준으로 나누기
    while ((pos = paramCopy.find(delim, start)) != std::string::npos) {
        // ':'를 만나면 루프를 종료하고 이후 문자열을 결과에 추가
        if (paramCopy[start] == ':') {
            res.push_back(paramCopy.substr(start + 1));
            return res;
        }

        res.push_back(paramCopy.substr(start, pos - start));
        start = pos + 1;
    }

    // 마지막 남은 부분 확인 및 ':' 처리
    if (start < paramCopy.size())
        res.push_back(paramCopy.substr(start));
	

    return res;
}


void Cmd::cmdKick() {
	// 명령어를 보낸 클라이언트가 register 되지 않은 경우
    if (client->getRegisteredStatus() == false) {
        throw Cmd::CmdException(server.makeMsg(PREFIX_SERVER, ERR_NOTREGISTERED(client->getNickname())));
	}
		
	std::vector<std::string> params = split(' ');

	// 파라미터 부족/과다 (최소 2개, 최대 3개)
	if (params.size() < 2 || params.size() > 3)
		throw Cmd::CmdException(server.makeMsg(PREFIX_SERVER, ERR_NEEDMOREPARAMS(client->getNickname(), cmd)));

	std::string chName = params[0];
	std::string target = params[1];
	std::string comment = "";

	if (params.size() == 3)
		std::string comment = params[2];
	
	// 채널 이름의 형식이 잘못된 경우
	if (chName[0] != '#') 
		throw Cmd::CmdException(server.makeMsg(PREFIX_SERVER, ERR_BADCHANMASK(client->getNickname(), chName)));

	// 채널이 존재하지 않는 경우
	if (server.getChannels().find(chName) == server.getChannels().end())
		throw Cmd::CmdException(server.makeMsg(PREFIX_SERVER, ERR_NOSUCHCHANNEL(client->getNickname(), chName)));

	std::map<std::string, Channel*> chs = server.getChannels();
	std::map<std::string, Channel*>::iterator it = chs.find(chName);
	Channel* ch = it->second;

	// 명령어를 호출한 클라이언트가 채널에 참여한 클라이언트가 아닌경우
	if (ch->getParticipant().find(ch->isOperatorNickname(client->getNickname())) == ch->getParticipant().end())
		throw Cmd::CmdException(server.makeMsg(PREFIX_SERVER, ERR_NOTONCHANNEL(client->getNickname(), chName)));

	// 채널 오퍼레이터 권한이 없는 경우
	if (ch->isOperator(client->getNickname()) == false)	
		throw Cmd::CmdException(server.makeMsg(PREFIX_SERVER, ERR_CHANOPRIVSNEEDED(client->getNickname(), chName)));

	// 강퇴시킬 사용자가 채널의 참여자가 아닌 경우
	if (ch->getParticipant().find(ch->isOperatorNickname(target)) == ch->getParticipant().end())
		throw Cmd::CmdException(server.makeMsg(PREFIX_SERVER, ERR_USERNOTINCHANNEL(client->getNickname(), target, chName)));
	
	// 강퇴 되었다고 채널의 모든 참여자(강퇴 대상자 포함)에게 알림 (-1: 강퇴자 포함 채널의 모든 참여자에게 알림)
	server.broadcastMsg(server.makeMsg(client->getPrefix(), RPL_KICK(chName, target, comment)), ch, -1);
	
	// 알림 발송 됐으면 강퇴
	ch->removeParticipant(ch->isOperatorNickname(target));

	// add : sejkim2 (운영자일 경우 운영자 목록에서도 삭제)
	// fix : sejkim2 (client->getNickname이 아니라 target이 나가아 함)
	if (ch->isOperator(target) == true)
        ch->removeOperator(target);

	if (ch->getParticipant().empty())
		server.removeChannel(chName);
}
