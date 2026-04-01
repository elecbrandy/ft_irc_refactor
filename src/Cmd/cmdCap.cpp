#include "Cmd.hpp"

void Cmd::cmdCap() {
	server.castMsg(client_fd, server.makeMsg(client->getPrefix(), "CAP * LS :"));
}
