#ifndef SERVERMSG_HPP
# define SERVERMSG_HPP

/* COUT LOG */
# define MSG_SERVER_INIT(port)			"IRC Server INIT!: port: " + port
# define MSG_NEW_CONNECTED(fd)			"new client connected [fd: " + fd + "]"
# define MSG_END_CONNECTED(fd)			"client connection closed [fd: " + fd + "]"
# define MSG_CONNECTION_TIMEOUT(fd)		"client connection timed out [fd: " + fd + "]"
# define MSG_CHECK_CONNECTION_START		"checking client connection..."
# define MSG_RECV_SIGNAL         		"signal received."
# define MSG_SERVER_SHUTDOWN				"server closed."

/* !ERROR! */
# define	ERR_ARG_COUNT			"invalid argument count. (only 3)"
# define	ERR_PORT_NULL			"port number is NULL or empty."
# define	ERR_PORT_DIGIT			"port number can only contain numbers."
# define	ERR_PORT_RANGE			"port number is out of valid range."
# define	ERR_PASSWORD_NULL		"password is NULL or empty."
# define ERR_PASSWORD_SIZE		"password must be less than 10 characters."
# define	ERR_PASSWORD_ALNUM		"password can only contain alphabet or number."

# define	ERR_SOCKET_CREATION	    "socket creation error."
# define	ERR_SOCKET_OPTIONS		"socket options setting error."
# define	ERR_SOCKET_BIND			"socket binding error."
# define	ERR_SOCKET_LISTEN		"socket listening error."
# define	ERR_SET_NONBLOCKING		"failed to set socket to non-blocking mode."
# define	ERR_ACCEPT_CLIENT		"client acceptance error."
# define	ERR_CLIENT_NONBLOCKING	"failed to set client socket to non-blocking mode."
# define	ERR_DATA_RECEIVE		"data reception error."
# define	ERR_SEND				"send() error."
# define ERR_CLIENT_FD           "client fd error."
# define	ERR_POLL				"poll() error."
# define	ERR_ETC					"unknown error occurred."
# define ERR_RECV				"recv() error."
# define ERR_OPEN_FILE           "cannot open file."
# define ERR_BUFFER_SIZE         "buffer size error."

#endif
