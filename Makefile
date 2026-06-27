NAME    = ircserv

CPP     = c++
CPPFLAGS = -Wall -Wextra -Werror -std=c++98

INCS    = .
SRCS_CMD = $(wildcard src/Cmd/*.cpp)
SRCS_MAIN = $(wildcard src/*.cpp)
SRCS    = $(SRCS_MAIN) $(SRCS_CMD)
HDRS    = $(wildcard src/*.hpp)
OBJS    = $(SRCS:.cpp=.o)
TEST_SCRIPT = test/run_all.py

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

# 메모리 안전망: 메인 루프/생명주기 작업 시에만 사용 (use-after-free 탐지)
# -std=c++98 은 유지하되 sanitizer 만 얹는다. 평소 빌드/평가에는 쓰지 않음.
asan: fclean
	$(MAKE) all CPPFLAGS="-Wall -Wextra -std=c++98 -g -fsanitize=address,undefined"

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
	@pkill $(NAME) || true
	@# 서버 실행 (로그를 터미널에 직접 쏘거나, 명확하게 리다이렉션)
	@./$(NAME) $(IRC_PORT) $(IRC_PASSWORD) > test_server.log 2>&1 & \
		SERVER_PID=$$!; \
		echo "Server started with PID: $$SERVER_PID"; \
		sleep 2; \
		python3 $(TEST_SCRIPT); \
		TEST_EXIT_CODE=$$?; \
		echo "Test finished with exit code: $$TEST_EXIT_CODE"; \
		kill $$SERVER_PID; \
		exit $$TEST_EXIT_CODE

.PHONY: all clean fclean re asan test up down restart log status clean-docker