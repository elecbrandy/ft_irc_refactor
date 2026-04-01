NAME    = ircserv

CPP     = c++
CPPFLAGS = -Wall -Wextra -Werror -std=c++98

INCS    = .
SRCS_CMD = $(wildcard src/Cmd/*.cpp)
SRCS_MAIN = $(wildcard src/*.cpp)
SRCS    = $(SRCS_MAIN) $(SRCS_CMD)
HDRS    = $(wildcard src/*.hpp)
OBJS    = $(SRCS:.cpp=.o)
TEST_SCRIPT = tests/test_basic.py

# -------------------------------------------------------
# Local Build
# -------------------------------------------------------

all: $(NAME)

$(NAME): $(OBJS)
	$(CPP) $(CPPFLAGS) $(OBJS) -o $(NAME) -I$(INCS)

%.o: %.cpp $(HDRS)
	$(CPP) $(CPPFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME)

re: fclean all

# -------------------------------------------------------
# Docker
# -------------------------------------------------------

# .env가 없을 경우 기본값 사용
-include .env
IRC_PORT     ?= 6667
IRC_PASSWORD ?= password

up:
	IRC_PORT=$(IRC_PORT) IRC_PASSWORD=$(IRC_PASSWORD) docker compose up --build -d

down:
	IRC_PORT=$(IRC_PORT) IRC_PASSWORD=$(IRC_PASSWORD) docker compose down

restart:
	IRC_PORT=$(IRC_PORT) IRC_PASSWORD=$(IRC_PASSWORD) docker compose down
	IRC_PORT=$(IRC_PORT) IRC_PASSWORD=$(IRC_PASSWORD) docker compose up --build -d

log:
	docker compose logs -f

status:
	docker compose ps

clean-docker:
	docker compose down --rmi all --volumes --remove-orphans

test: $(NAME)
	@echo "$(GREEN)Starting local integration tests...$(RESET)"
	@# 1. 혹시 이미 실행 중인 서버가 있다면 종료
	@pkill $(NAME) || true
	@# 2. 서버를 백그라운드에서 실행 (로그는 test_server.log에 기록)
	@./$(NAME) $(IRC_PORT) $(IRC_PASSWORD) > test_server.log 2>&1 & \
		SERVER_PID=$$!; \
		echo "Server started with PID: $$SERVER_PID"; \
		sleep 1; \
		# 3. 파이썬 테스트 실행 \
		python3 $(TEST_SCRIPT); \
		TEST_EXIT_CODE=$$?; \
		# 4. 테스트 종료 후 서버 프로세스 정리 \
		kill $$SERVER_PID; \
		exit $$TEST_EXIT_CODE

.PHONY: all clean fclean re up down restart log status clean-docker