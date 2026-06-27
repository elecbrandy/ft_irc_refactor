#include "../../include/Cmd.hpp"

void Cmd::cmdQuit() {
	Client* client = server.getClient(client_fd);
	if (client) {
		server.markClientForRemoval(client_fd);	// 지연 삭제: cleanupMarkedClients가 정리
	}
}
