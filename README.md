# ft_irc

![ft_irc main image](./img/irc_main.png)

<br>

## 📌 Overview

> _**Internet Relay Chat**_

- **프로젝트 목표**: C++98 기반 IRC 서버의 구조 개선, 메모리 안정성 확보, 테스트 자동화
- **주요 범위**: `poll()` 기반 이벤트 루프, IRC 명령어 처리, 채널/사용자 상태 관리

<br>
<br>

## 🧩 `ft_irc` 란?

- `ft_irc`는 [IRC(Internet Relay Chat)](https://en.wikipedia.org/wiki/Internet_Relay_Chat) 프로토콜을 직접 구현하는 프로젝트입니다.
- 이 저장소는 소켓 프로그래밍과 네트워크 프로토콜의 기본 구조를 유지하면서, 클라이언트 생명주기와 채널 상태 관리 로직을 더 안전하게 정리하는 것을 목표로 합니다.

<br>
<br>

## 🏗️ System Architecture

![ft_irc system architecture](./img/irc_arch.png)

`ircserv`는 단일 스레드 이벤트 기반 IRC 서버입니다. 모든 클라이언트 I/O는 하나의 `poll()` 루프에서 처리되고, IRC 명령어는 `Cmd` 계층에서 파싱 및 실행됩니다. 서버 상태는 메모리에만 존재하며, 클라이언트와 채널 정보가 단일 source of truth 역할을 합니다.

| Layer | Responsibility |
|---|---|
| I/O | 클라이언트 접속 수락, non-blocking read/write, send buffer 관리 |
| Protocol | IRC 메시지 파싱, 명령어 dispatch, numeric reply 생성 |
| State | 클라이언트, 채널, 닉네임 인덱스, 채널 모드 관리 |

<br>
<br>

## 📂 Project Structure

```bash
.
├── src/               # 서버 구현 및 IRC 명령어 처리
│   └── Cmd/           # PASS, NICK, USER, JOIN, MODE 등 명령어 구현
├── include/           # 헤더 파일
├── conf/              # MOTD 등 서버 설정 파일
├── docs/              # 아키텍처 및 리팩토링 기록
├── img/               # README 이미지
├── test/              # Python 기반 통합 테스트
├── Makefile
├── Dockerfile
└── docker-compose.yml
```

<br>
<br>

## 🛠️ Tech Stack

| Area | Stack |
|---|---|
| Language | C++98 |
| Network I/O | TCP socket, non-blocking I/O, `poll()` |
| Build | Makefile |
| Test | Python integration test |
| Runtime | Local binary, Docker Compose |
| Debug | AddressSanitizer, UndefinedBehaviorSanitizer, `leaks` |

<br>
<br>

## 🚀 Quick Start

### 1. 환경 설정

`.env` 파일을 생성해 포트와 패스워드를 지정할 수 있습니다. 파일이 없으면 기본값 `6667` / `password`가 사용됩니다.

```dotenv
IRC_PORT=6667
IRC_PASSWORD=password
```

### 2. Docker로 실행

```bash
make up
```

### 3. IRC 클라이언트 접속

```bash
Host     : localhost
Port     : 6667
Password : password
```

`nc`로도 빠르게 연결을 확인할 수 있습니다.

```bash
nc localhost 6667
```

<br>
<br>

## ⚙️ Commands

### Docker

| Command | Description |
|---|---|
| `make up` | 이미지 빌드 후 서버 시작 |
| `make down` | 서버 중지 및 컨테이너 제거 |
| `make restart` | 서버 재빌드 후 재시작 |
| `make log` | 실시간 로그 출력 |
| `make status` | 컨테이너 상태 확인 |
| `make clean-docker` | 컨테이너, 이미지, 볼륨 전체 삭제 |

### Local

| Command | Description |
|---|---|
| `make` | `ircserv` 바이너리 빌드 |
| `make re` | 전체 재빌드 |
| `make clean` | 오브젝트 파일 삭제 |
| `make fclean` | 오브젝트 파일과 바이너리 삭제 |
| `make asan` | AddressSanitizer / UBSan 옵션으로 빌드 |
| `make test` | Python 기반 통합 테스트 실행 |

<br>
<br>

## 💬 Supported IRC Features

| Category | Features |
|---|---|
| Registration | `PASS`, `NICK`, `USER`, registration state 관리 |
| Connection | `PING`, `QUIT`, timeout, partial packet buffering |
| Channel | `JOIN`, `PART`, channel participant/operator 관리 |
| Messaging | `PRIVMSG`, channel broadcast, direct message |
| Operator | `KICK`, `INVITE`, `TOPIC`, `MODE` |
| Channel Mode | `+i`, `+t`, `+k`, `+o`, `+l` |

<br>
<br>

## 🧪 Refactoring Focus

| Area | Summary |
|---|---|
| Client lifecycle | 모든 삭제 경로를 mark 후 단일 cleanup 지점에서 정리하도록 개선 |
| Non-blocking output | partial write와 `EAGAIN` 상황을 send buffer로 처리 |
| Channel state | participant key를 순수 닉네임으로 통일하고 operator 상태를 별도 관리 |
| Error handling | 등록된 클라이언트의 명령어 에러가 연결 종료로 이어지지 않도록 수정 |
| Test safety net | 등록, 운영자 명령, 생명주기 테스트를 Python runner로 자동화 |

<br>
<br>

## 👥 Team

| 팀원 | 김동우 | 김세진 | 최란 |
|---|---|---|---|
| 역할 | `Server / Infra` | `CMD / Option` | `Channel / User` |

_*사람이 작성함_
