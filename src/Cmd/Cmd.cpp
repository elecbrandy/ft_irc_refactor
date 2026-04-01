#include "Cmd.hpp"

Cmd::Cmd(IrcServer &s, std::string &msg, int client_fd)
: server(s), client_fd(client_fd), client(server.getClient(client_fd)), msg(msg) {}

Cmd::~Cmd() {}

const char* Cmd::CmdException::what() const throw() {return msg.c_str();}
std::string Cmd::getCmdParams() const {return this->cmdParams;}

std::string Cmd::extractCmd() {
	size_t pos = 0;

	while (pos < msg.size() && !std::isspace(static_cast<unsigned char>(msg[pos]))) {
		++pos;
	}
	return msg.substr(0, pos);
}

std::string Cmd::extractCmdParams() {
	size_t startPos = this->cmd.size();

	if (msg.size() > startPos && std::isspace(static_cast<unsigned char>(msg[startPos]))) {
		++startPos;
	}

	if (startPos < msg.size()) {
		return msg.substr(startPos);
	} else {
		return "";
	}
}

bool Cmd::handleClientCmd() {
	try {
		this->cmd = extractCmd();
		this->cmdParams = extractCmdParams();

		if (cmd == "CAP")
			cmdCap();
		else if (cmd == "NICK")
			cmdNick();
		else if (cmd == "PASS")
			cmdPass();
		else if (cmd == "USER")
			cmdUser();
		else if (cmd == "PING")
			cmdPing();
		else if (cmd == "JOIN")
			cmdJoin();
		else if (cmd == "PART")
			cmdPart();
		else if (cmd == "PRIVMSG")
			cmdPrivmsg();
		else if (cmd == "KICK")
			cmdKick();
		else if (cmd == "INVITE")
			cmdInvite();
		else if (cmd == "MODE")
			cmdMode();
		else if (cmd == "TOPIC")
			cmdTopic();
		else if (cmd == "QUIT")
			cmdQuit();
		else
			return false;
	} catch (const CmdException& e) {
		server.castMsg(client_fd, e.what());
		if (!client->getPassStatus() || !client->getRegisteredStatus()) {
			server.removeClientFromServer(client);
		}
		return false;
	}
	return true;
}

void Cmd::printWelcome() {
	// RPL
	server.castMsg(client_fd, server.makeMsg(PREFIX_SERVER, RPL_WELCOME(client->getNickname(), client->getServername())));
	server.castMsg(client_fd, server.makeMsg(PREFIX_SERVER, RPL_YOURHOST(client->getNickname(), client->getServername())));
	server.castMsg(client_fd, server.makeMsg(PREFIX_SERVER, RPL_CREATED(client->getNickname(), server.formatDateToString(server.getStartTime()))));
	server.castMsg(client_fd, server.makeMsg(PREFIX_SERVER, RPL_MYINFO(client->getNickname())));
	
	// MOTD
	server.castMsg(client_fd, server.makeMsg(PREFIX_SERVER, RPL_MOTDSTART(client->getNickname())));
	std::ifstream goatFile(PATH_MOTD);
	if (goatFile.is_open()) {

		std::string line;
		while (getline(goatFile, line)) {
			server.castMsg(client_fd, server.makeMsg(PREFIX_SERVER, RPL_MOTD(client->getNickname(), line)));
		}
		goatFile.close();
	}
	server.castMsg(client_fd, server.makeMsg(PREFIX_SERVER, RPL_ENDOFMOTD(client->getNickname())));
}
