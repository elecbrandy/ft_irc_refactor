#include "Cmd.hpp"

// /*
// 명령어 : PART <channel>{,<channel>}

// ERR_NEEDMOREPARAMS
// ERR_NOSUCHCHANNEL
// ERR_NOTONCHANNEL

// - 채널에서 나가는 명령어
// - 여러 채널을 나가려면 채널 이름을 콤마로 구분하여 나열

// 경우의 수
// 1. 파라미터 부족 : 파라미터가 없는 경우 (최소 1개 있어야함)
// 2. 해당 채널이 존재하지 않는 경우(#으로 시작하지 않는 경우와 그냥 존재하지 않는 경우)
// 3. 채널은 존재하지만 사용자가 해당 채널에 참여하지 않은 경우
// */

void Cmd::cmdPart() {
    // 명령어를 보낸 클라이언트가 register 되지 않은 경우
    if (client->getRegisteredStatus() == false) {
        throw Cmd::CmdException(server.makeMsg(PREFIX_SERVER, ERR_NOTREGISTERED(client->getNickname())));
    }
        
    std::vector<std::string> params = split(',');

    if (params.empty())
        throw Cmd::CmdException(server.makeMsg(PREFIX_SERVER, ERR_NEEDMOREPARAMS(client->getNickname(), cmd)));

    for (size_t i = 0; i < params.size(); i++) {
        std::string chName = params[i];

        // 파라미터로 들어온 채널이름 형식이 잘못된 경우
        if (isValidChannelName(chName) == false) {
            server.castMsg(client_fd, server.makeMsg(PREFIX_SERVER, ERR_BADCHANMASK(client->getNickname(), chName)));
            continue ;
        }

        // 채널이 존재하지 않는 경우
        std::map<std::string, Channel*> chs = server.getChannels();

        if (chs.find(chName) == chs.end()) {
            server.castMsg(client_fd, server.makeMsg(PREFIX_SERVER, ERR_NOSUCHCHANNEL(client->getNickname(), chName)));
            continue ;
        }

        // 채널은 존재하지만 사용자가 해당 채널에 참여하지 않은 경우
        Channel* ch = chs[chName];
        std::string n = ch->isOperatorNickname(this->client->getNickname());
        if (ch->getParticipant().find(n) == ch->getParticipant().end()) {
            server.castMsg(client_fd, server.makeMsg(PREFIX_SERVER, ERR_NOTONCHANNEL(client->getNickname(), chName)));
            continue ;
        }

        // 채널에서 나간다고 채널의 모든 참여자에게 알람 후 채널에서 사용자를 제거
        server.broadcastMsg(server.makeMsg(client->getPrefix(), RPL_PART(chName)), ch, -1);
        ch->removeParticipant(ch->isOperatorNickname(client->getNickname()));

        // add : sejkim2 (participant 목록 갱신 후 operator 목록도 갱신)
        if (ch->isOperator(client->getNickname()) == true)
            ch->removeOperator(client->getNickname());
        
        // 참여자가 나간 뒤, 채널에 참여자가 없으면 채널을 삭제한다.
        if (ch->getParticipant().empty())
            server.removeChannel(chName);
    }
}
