#ifndef ARGPARSER_HPP
#define ARGPARSER_HPP

#include <iostream>
#include <string>
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include "Server.hpp"
#include "ServerCode.hpp"

class ArgParser {
private:
	int			port;
	std::string	password;
public:
	ArgParser();
	ArgParser(int ac, char **av);
	~ArgParser();

	/* arg */
	void		setArg(int ac, char **av);

	/* port */
	int			getPort() const;
	void		setPort(char *s);
	int			checkPort(char *s);

	/* password */
	std::string	getPassword() const;
	void		setPassword(char *s);
	std::string	checkPassword(char *s);

	/* exception */
	class ArgException : public std::exception {
	private:
		std::string	msg;
	public:
		ArgException(std::string str) : msg(str) {}
		virtual ~ArgException() throw() {};
		virtual const char*	 what() const throw();
	};
};

#endif
