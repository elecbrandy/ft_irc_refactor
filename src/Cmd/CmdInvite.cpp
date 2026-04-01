#include "Cmd.hpp"

/*
명령어 : <nickname> <channel>

ERR_NEEDMOREPARAMS - 최소 2개의 파라미터가 필요함
ERR_NOSUCHNICK - 초대 당하는 사용자가 서버에 존재하지 않는 경우
ERR_NOTONCHANNEL - 초대를 보내는 사용자가 해당 채널의 참여자가 아닌 경우
ERR_USERONCHANNEL - 초대 당하는 사용자가 이미 해당 채널의 참여자인 경우
ERR_CHANOPRIVSNEEDED - 채널이 +i 일때, 초대를 보내는 사용자가 채널 오퍼레이터가 아닌 경우
RPL_INVITING - 초대를 보내는 사용자에게 초대가 성공적으로 전송됐음을 알림
(x)RPL_AWAY - 초대 당하는 사용자가 away 상태인 경우 (구현x)

경우의 수
1.파라미터 부족/과다 : 2개의 파라미터가 필요함
3.초대를 보내는 사용자가 해당 채널의 참여자가 아닌 경우
2.초대 당하는 사용자가 서버에 존재하지 않는 경우
4.초대 당하는 사용자가 이미 해당 채널의 참여자인 경우(중복 참여 같은 비정상 적인 동작을 방지하기 위함)
5.채널이 +i 일때, 초대를 보내는 사용자가 채널 오퍼레이터가 아닌 경우
*/

void Cmd::cmdInvite() {
    // 명령어를 보낸 클라이언트가 register 되지 않은 경우
    if (client->getRegisteredStatus() == false) {
        throw Cmd::CmdException(server.makeMsg(PREFIX_SERVER, ERR_NOTREGISTERED(client->getNickname())));
    }
        
    std::vector<std::string> params = split(' ');
    
    // 파라미터 부족/과다
    if (params.size() != 2)
        throw Cmd::CmdException(server.makeMsg(PREFIX_SERVER, ERR_NEEDMOREPARAMS(client->getNickname(), cmd)));
    
    std::string target = params[0];
    std::string chName = params[1];

    // 채널이름 형식이 잘못된 경우
    if (isValidChannelName(chName) == false)
        throw Cmd::CmdException(server.makeMsg(PREFIX_SERVER, ERR_BADCHANMASK(client->getNickname(), chName)));
    
    // 채널이 존재하지 않는 경우
    std::map<std::string, Channel*> chs = server.getChannels();
    if (chs.find(chName) == chs.end())
        throw Cmd::CmdException(server.makeMsg(PREFIX_SERVER, ERR_NOSUCHNICK(client->getNickname(), chName)));

    // 초대를 보내는 사용자가 해당 채널의 참여자가 아닌 경우
    Channel* ch = chs[chName];
    std::string nick = ch->isOperatorNickname(client->getNickname());
    if (ch->getParticipant().find(nick) == ch->getParticipant().end())
        throw Cmd::CmdException(server.makeMsg(PREFIX_SERVER, ERR_NOTONCHANNEL(client->getNickname(), chName)));

    // 채널이 +i 일때, 초대를 보내는 사용자가 채널 오퍼레이터가 아닌 경우
    if (ch->getMode().find(INVITE_MODE) != ch->getMode().end() && ch->isOperator(client->getNickname()) == false)
        throw Cmd::CmdException(server.makeMsg(PREFIX_SERVER, ERR_CHANOPRIVSNEEDED(client->getNickname(), chName)));

    // 초대 당하는 사용자가 서버에 존재하지 않는 경우
    if (server.getClient(target) == NULL)
        throw Cmd::CmdException(server.makeMsg(PREFIX_SERVER, ERR_NOSUCHNICK(client->getNickname(), target)));

    // 초대 당하는 사용자가 이미 해당 채널의 참여자인 경우
    if (ch->getParticipant().find(ch->isOperatorNickname(target)) != ch->getParticipant().end())
        throw Cmd::CmdException(server.makeMsg(PREFIX_SERVER, ERR_USERONCHANNEL(target, chName)));

    // 채널의 초대 목록에 추가 (초대 목록에 없는 사용자만 추가)
    if (std::find(ch->getInvited().begin(), ch->getInvited().end(), target) == ch->getInvited().end())
        ch->addInvited(target);

    // 초대된 사용자에게 초대 메시지 전송
    server.castMsg(server.getClient(target)->getFd(), server.makeMsg(client->getPrefix(), RPL_INVITE(target, chName)));

    // 초대한 사용자에게 초대 메시지 전송
    server.castMsg(client_fd, server.makeMsg(PREFIX_SERVER, RPL_INVITING(client->getNickname(), chName, target)));
}
