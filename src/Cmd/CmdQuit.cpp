#include "Cmd.hpp"

void Cmd::cmdQuit() {
	Client* client = server.getClient(client_fd);
	if (client) {
		server.removeClientFromServer(client);
	}
}
