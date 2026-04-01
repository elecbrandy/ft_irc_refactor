#ifndef ERRORCODE_HPP
#define ERRORCODE_HPP

enum ErrorCode {
    /* ArgParser */
    ERR_ARG_COUNT = 1,
    ERR_PORT_NULL,
    ERR_PORT_DIGIT,
    ERR_PORT_RANGE,
    ERR_PASSWORD_NULL,
    ERR_PASSWORD_SIZE,
    ERR_PASSWORD_ALNUM,

    /* Sever */
    ERR_SOCKET_CREATION,		// 소켓 생성 에러
    ERR_SOCKET_OPTIONS,				// 소켓 옵션 설정 에러
    ERR_SOCKET_BIND,				// 소켓 바인딩 에러
    ERR_SOCKET_LISTEN,				// 소켓 리슨 에러
    ERR_SET_NONBLOCKING,			// 논블로킹 모드 설정 에러
    ERR_ACCEPT_CLIENT,				// 클라이언트 수락 에러
    ERR_CLIENT_NONBLOCKING,			// 클라이언트 소켓 논블로킹 설정 에러
    ERR_DATA_RECEIVE,				// 데이터 수신 에러
    ERR_MESSAGE_SEND,				// 메시지 전송 에러
    ERR_POLL,						// poll() 에러
	ERR_ETC							// 이상한거 일단 여기에 다 넣기

};

#endif