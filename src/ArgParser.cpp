#include "ArgParser.hpp"

ArgParser::ArgParser() : port(-1), password(NULL) {}

ArgParser::ArgParser(int ac, char **av) {
	setArg(ac, av);
}

ArgParser::~ArgParser() {}

void ArgParser::setArg(int ac, char **av) {
	if (ac != 3) {
		throw ArgException(ERR_ARG_COUNT);
	} else {
		setPort(av[1]);
		setPassword(av[2]);
	}
}

int ArgParser::getPort() const {
	return this->port;
}

void ArgParser::setPort(char *s) {
	this->port = checkPort(s);
}

int ArgParser::checkPort(char *s) {
	/* NULL check */
	if (s == NULL || *s == '\0') {
		throw ArgException(ERR_PORT_NULL);
	}

	/* DIGIT check */
	for (int i = 0; s[i] != '\0'; ++i) {
		if (!std::isdigit(static_cast<unsigned char>(s[i]))) {
			throw ArgException(ERR_PORT_DIGIT);
		}
	}

	int tmp = std::atoi(s);

	/* MIN, MAX, LEN check */
	if (PORT_MIN <= tmp && tmp <= PORT_MAX && std::strlen(s) <= PORT_MAX_LEN) {
		return tmp;
	} else {
		throw ArgException(ERR_PORT_RANGE);
	}
}

std::string ArgParser::getPassword() const {
	return this->password;
}

void ArgParser::setPassword(char *s) {
	this->password = checkPassword(s);
}

std::string ArgParser::checkPassword(char *s) {
	/* NULL check */
	if (s == NULL || *s == '\0') {
		throw ArgException(ERR_PASSWORD_NULL);
	}

	std::string password(s);
	
	/* SIZE check */
	if (password.size() >= PASSWORD_MAX_LEN) {
		throw ArgException(ERR_PASSWORD_SIZE);
	}
	
	/* ALNUM check */
	for (std::string::const_iterator it = password.begin(); it != password.end(); ++it) {
		if (!std::isalnum(static_cast<unsigned char>(*it))) {
			throw ArgException(ERR_PASSWORD_ALNUM);
		}
	}
	return password;
}

const char* ArgParser::ArgException::what() const throw() {
	return msg.c_str();
}
