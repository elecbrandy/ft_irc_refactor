#include "Cmd.hpp"

void Cmd::cmdPing() {
    // 명령어를 보낸 클라이언트가 register 되지 않은 경우
    if (client->getRegisteredStatus() == false) {
        throw Cmd::CmdException(server.makeMsg(PREFIX_SERVER, ERR_NOTREGISTERED(client->getNickname())));
    }

    // 클라이언트의 PING 요청 처리 (현재 시각으로 갱신)
    client->updateLastPingSent();

    // 서버에서 응답 PING 전송
    server.castMsg(client_fd, server.makeMsg(PREFIX_SERVER, RPL_PING(getCmdParams())));
}