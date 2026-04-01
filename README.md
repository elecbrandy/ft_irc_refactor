# ft_irc_refactor

> 이 Repository는 `2024.10-11` 진행했던 [ft_irc](https://github.com/elecbrandy/ft_irc) 프로젝트를 리팩토링하기 위해 제작되었습니다.

`ft_irc`는 [IRC(Internet Relay Chat)](https://en.wikipedia.org/wiki/Internet_Relay_Chat) 프로토콜을 직접 구현하는 프로젝트입니다. 이 프로젝트의 목표는 **소켓 프로그래밍과 네트워크 프로토콜의 기본 원리를 이해** 하고, 클라이언트-서버 구조, 멀티플렉싱, 채널/사용자 관리 및 명령어 처리 로직을 직접 구현하는 것입니다.

<br>
<br>

## 프로젝트 구조

``` bash
.
├── src/               # 소스 코드
│   └── Cmd/           # IRC 명령어 구현
├── conf/              # 서버 설정 파일
│   ├── motd.txt       # 접속 시 출력되는 MOTD
│   └── goat.txt       # 서버 시작 시 출력되는 아스키아트
├── Makefile
├── Dockerfile.server
├── docker-compose.yml
└── .env               # 포트/패스워드 설정 (없으면 기본값 사용)
```

<br>
<br>

## 빠른 시작

### 1️⃣ 환경설정

- `.env` 파일을 생성해 포트와 패스워드를 지정
- 파일이 없으면 기본값(`6667` / `password`)이 자동으로 사용됨

```dotenv
IRC_PORT=6667
IRC_PASSWORD=password
```

<br>

### 2️⃣ 서버 실행

```bash
make up
```

<br>

### 3️⃣ IRC 클라이언트 접속

- 서버가 실행된 후 아무 IRC 클라이언트로 접속

``` bash
Host     : localhost
Port     : 6667  (또는 .env에 지정한 포트)
Password : password  (또는 .env에 지정한 패스워드)
```

- 또는 `nc`로 빠르게 연결 테스트

```bash
nc localhost 6667
```

<br>
<br>

## 명령어

### 1️⃣ Make 명령어 (Docker Container)

| 명령어 | 설명 |
|---|---|
| `make up` | 이미지 빌드 후 서버 시작 (백그라운드) |
| `make down` | 서버 중지 및 컨테이너 제거 |
| `make restart` | 서버 재빌드 후 재시작 |
| `make log` | 실시간 로그 출력 (`Ctrl+C`로 종료) |
| `make status` | 컨테이너 상태 확인 |
| `make clean-docker` | 컨테이너, 이미지, 볼륨 전체 삭제 |

<br>

### 2️⃣ 로컬 빌드

| 명령어 | 설명 |
|---|---|
| `make local-build` | 바이너리 빌드 |
| `make local-run` | 빌드 후 바로 실행 |
| `make local-clean` | 오브젝트 파일 삭제 |
| `make local-fclean` | 오브젝트 파일 + 바이너리 삭제 |
| `make local-re` | 전체 재빌드 |

<br>
<br>

## 개발 관련

### 1️⃣ Memory Leak Check

```bash
while True; do leaks ircserv | grep leaked; sleep 1; done;
```

<br>

### 2️⃣ 요구사항 체크리스트

- **Server 기본 구현**
  - [x] Client - Server 기본 연결
  - [x] 닉네임 및 사용자명 설정 가능
  - [x] 수신받은 패킷 이어 붙이기 (`com^Dman^Dd`)
- **Cmd**
  - [x] `PASS`
  - [x] `USER`
  - [x] `PING`
  - [x] `NICK`
- **Channel 기본 구현**
  - [x] 클라이언트 채널 참여 가능
  - [x] 클라이언트가 채널에 보낸 모든 메세지는 같은 채널에 속한 다른 클라이언트에게 전달
  - [x] 클라이언트는 운영자(`operators`) / 일반 사용자(`regular users`)로 구분
- **Channel 내 `operators` 전용 명령어**
  - [x] `KICK` : 채널에서 클라 강제퇴장
  - [x] `INVITE` : 특정 클라를 채널로 초대
  - [x] `TOPIC` : 채널 주제 변경/조회
  - [x] `MODE` : 채널 모드 설정/변경
    - `i` : 초대 전용 채널 설정/해제  
    - `t` : 운영자만 채널 주제 변경 가능  
    - `k` : 채널 비밀번호 설정/해제  
    - `o` : 운영자 권한 부여/박탈  
    - `l` : 채널 인원 제한 설정/해제

<br>

### 3️⃣ 참고 사항

- **`recv` 의 반환값**
  - 리눅스 `man` 페이지에 따르면, 논블로킹 소켓에서 `recv` 호출 시 읽을 데이터가 없으면 `-1`을 반환하고 `errno`에 `EAGAIN` 또는 `EWOULDBLOCK`을 설정함
  - 읽기(`recv`): 수신 버퍼에 데이터가 없을 때 `-1` 반환 + `errno = EAGAIN` 설정
  - 쓰기(`send`): 송신 버퍼가 가득 차 있을 때 `-1` 반환 + `errno = EAGAIN` 설정
  - 대처 방법: 이벤트 루프에서 대기 후 재시도
- **POLLIN과 POLLOUT**
  - `POLLIN` 값: `0x0001` (이진수 `0001`)
  - `POLLOUT` 값: `0x0004` (이진수 `0100`)
  - `POLLIN | POLLOUT = 0x0005`

<br>