# ft_irc

`ft_irc`는 [IRC(Internet Relay Chat)](https://en.wikipedia.org/wiki/Internet_Relay_Chat) 프로토콜을 직접 구현하는 프로젝트입니다. 이 프로젝트의 목표는 **소켓 프로그래밍과 네트워크 프로토콜의 기본 원리를 이해** 하고, 클라이언트-서버 구조, 멀티플렉싱, 채널/사용자 관리 및 명령어 처리 로직을 직접 구현하는 것입니다.

<br>

## 테스트
### Memory Leak 체크
개발 중 메모리 누수를 확인할 때는 아래 명령어로 체크할 것.

```bash
while True; do leaks ircserv | grep leaked; sleep 1; done;
```

### Docker를 통해 일관된 테스트 환경 구현

1. **Docker Desktop 실행**
   ```bash
   open -a Docker
   ```

2. `main` 브랜치에 있는 **setDocker.sh** 실행
   ```bash
   ./setDocker.sh
   ```

3. 터미널 창을 두 개 열고 각각 실행!
   - **서버 실행**
     ```bash
     docker exec -it irc_container inspircd --runasroot --nofork --debug
     ```
   - **클라이언트 실행**
     - 컨테이너에 접속 후 실행:
       ```bash
       docker exec -it irc_container /bin/bash
       irssi
       ```
     - 한 줄로 바로 실행:
       ```bash
       docker exec -it irc_container irssi -c host.docker.internal -p 6667 -w 1234 -n nickname
       ```

4. **컨테이너 종료**
   ```bash
   docker kill irc_container
   docker rm irc_container
   ```

<br>

## 체크리스트

### Server 기본 구현
- [x] Client - Server 기본 연결
- [x] 닉네임 및 사용자명 설정 가능
- [x] 수신받은 패킷 이어 붙이기 (`com^Dman^Dd`)

### Cmd
- [x] `PASS`
- [x] `USER`
- [x] `PING`
- [x] `NICK`

### Channel 기본 구현
- [x] 클라이언트 채널 참여 가능
- [x] 클라이언트가 채널에 보낸 모든 메세지는 같은 채널에 속한 다른 클라이언트에게 전달
- [x] 클라이언트는 운영자(`operators`) / 일반 사용자(`regular users`)로 구분

### Channel 내 `operators` 전용 명령어
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

## 기타 참고할 것

### `recv` 의 반환값
- 리눅스 `man` 페이지에 따르면, 논블로킹 소켓에서 `recv` 호출 시 읽을 데이터가 없으면 `-1`을 반환하고 `errno`에 `EAGAIN` 또는 `EWOULDBLOCK`을 설정함
- **읽기(`recv`)**: 수신 버퍼에 데이터가 없을 때 `-1` 반환 + `errno = EAGAIN` 설정
- **쓰기(`send`)**: 송신 버퍼가 가득 차 있을 때 `-1` 반환 + `errno = EAGAIN` 설정
- **대처 방법**: 이벤트 루프에서 대기 후 재시도

### POLLIN과 POLLOUT
- `POLLIN` 값: `0x0001` (이진수 `0001`)
- `POLLOUT` 값: `0x0004` (이진수 `0100`)
- `POLLIN | POLLOUT = 0x0005`
```

