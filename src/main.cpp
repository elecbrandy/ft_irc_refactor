#include "../include/Server.hpp"


static void signalHandler(int signal) {
	(void)signal;
	std::cerr << "SERV_LOG: " << C_ERR << MSG_RECV_SIGNAL << C_RESET;
	exit(EXIT_FAILURE);
}

int main(int ac, char** av) {
	// 인자 개수 체크
	if (ac != 3) {
		std::cerr << C_ERR << "Error: " << ERR_ARG_COUNT << C_RESET << std::endl;
		return 1;
	}

	// SIGPIPE 신호 무시 (클라이언트가 비정상적으로 연결 종료 시 발생)
	signal(SIGPIPE, SIG_IGN);

	// 서버 실행
	try {
		IrcServer server(av[1], av[2]);
		signal(SIGINT, signalHandler);
		signal(SIGQUIT, signalHandler);
		server.init();
		server.run();
	} catch (const IrcServer::ServerException &e) {
		std::cerr << C_ERR << "Error: " << e.what() << C_RESET << std::endl;
		return 1;
	}
	return 0;
}
