# IRC Server 루프 리팩토링 가이드

## 개요
이 가이드는 `Server.cpp`의 `run()` 메서드를 단계적으로 리팩토링하는 과정을 설명합니다.

---

## Phase 1: 준비 (기존 코드 분석)

### 현재 상태 확인
1. `src/Server.cpp`의 `run()` 메서드 (~150줄) 확인
2. `handleSocketRead()`, `handleSocketWrite()` 동작 이해
3. `removeClientFromServer()` 호출 위치 추적

### 주의사항
- `handleSocketRead()` 내에서 `Cmd::handleClientCmd()`가 false 반환하면 클라이언트 삭제됨
- `removeClientFromServer()` 호출 시 `_fds[i].fd`는 **변경되지 않음**
- 현재 `-1`로 무효화되는 시점: `removeClientFromServer()` 내부 (아님!) → **외부의 run() 루프 내**

---

## Phase 2: 함수 선언 추가

### Step 1: `include/Server.hpp` 수정

기존 코드:
```cpp
void									checkPingTimeOut();
std::string								makeMsg(const std::string& prefix, const std::string& msg);
```

추가할 내용:
```cpp
private:
	/* Event processing - separated for clarity */
	bool									processServerEvent();
	bool									processClientRead(int fd);
	bool									processClientWrite(int fd);
	
	/* Client cleanup - unified management */
	void									markClientForRemoval(int fd);
	void									cleanupMarkedClients();
	
	/* Server lifecycle */
	bool									shouldExitServer();
	bool									canRecover(const ServerException& e);
```

### 왜 private?
- 이 함수들은 `run()` 내부에서만 사용됨
- 외부 인터페이스가 아님
- 구현 세부사항을 숨길 수 있음

---

## Phase 3: 새 함수 구현

### Step 2: `src/Server.cpp`에 함수 추가

#### 2.1 processServerEvent()

**기존 코드:**
```cpp
if (current_fd == this->_fd) {
    if (_fds[i].revents & POLLIN) {
        acceptClient();
    }
    continue;
}
```

**개선된 코드:**
```cpp
bool IrcServer::processServerEvent() {
	try {
		acceptClient();
		return true;
	} catch (const ServerException& e) {
		// 연결 수락 실패는 일시적일 수 있음
		// 하지만 호출자에게 알려야 함
		serverLog(this->_fd, LOG_ERR, C_ERR, 
			"Failed to accept client: " + std::string(e.what()));
		return false;
	}
}
```

**이점:**
- 에러를 명시적으로 반환
- 호출자가 에러 처리 가능
- 로깅 일관성

#### 2.2 processClientRead(int fd)

**기존 코드:**
```cpp
if (_fds[i].revents & POLLIN) {
    handleSocketRead(current_fd);
}
```

**개선된 코드:**
```cpp
bool IrcServer::processClientRead(int fd) {
	Client* client = getClient(fd);
	if (!client) {
		return false;
	}

	char buffer[BUFFER_SIZE];
	size_t recvLen = recv(fd, buffer, BUFFER_SIZE - 1, 0);
	
	if (recvLen <= 0) {
		serverLog(fd, LOG_SERVER, C_WARN, 
			"Client disconnected or recv error");
		return false;
	}

	buffer[recvLen] = '\0';
	client->appendToRecvBuffer(buffer);
	
	std::string str;
	while (getClient(fd) && client->extractMessage(str)) {
		Cmd cmdHandler(*this, str, fd);
		serverLog(fd, LOG_INPUT, C_MSG, str);
		
		// handleClientCmd() 실패 시 클라이언트가 삭제됨
		if (!cmdHandler.handleClientCmd()) {
			return false;
		}
	}
	
	return true;
}
```

**왜 분리?**
- `handleSocketRead()` 로직을 복사하되 bool 반환 추가
- 기존 함수는 유지 (호환성)
- 새 함수는 더 명확한 인터페이스

**반환값 의미:**
- `true`: 클라이언트 정상 처리 (또는 유지)
- `false`: 클라이언트 삭제됨 → `markClientForRemoval()` 호출 필요

#### 2.3 processClientWrite(int fd)

```cpp
bool IrcServer::processClientWrite(int fd) {
	Client* client = getClient(fd);
	if (!client) {
		return false;
	}

	std::string& sendBuffer = client->getSendBuffer();

	if (!sendBuffer.empty()) {
		ssize_t sendLen = send(fd, sendBuffer.c_str(), sendBuffer.size(), 0);
		
		if (sendLen > 0) {
			sendBuffer.erase(0, sendLen);
		} else if (sendLen == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
			removeClientFromServer(client);
			return false;
		}
	}

	if (sendBuffer.empty()) {
		for (size_t i = 0; i < _fds.size(); ++i) {
			if (_fds[i].fd == fd) {
				_fds[i].events &= ~POLLOUT;
				break;
			}
		}
	}

	return true;
}
```

#### 2.4 markClientForRemoval(int fd)

```cpp
void IrcServer::markClientForRemoval(int fd) {
	for (size_t i = 0; i < _fds.size(); ++i) {
		if (_fds[i].fd == fd) {
			_fds[i].fd = -1;
			break;
		}
	}
}
```

**중요:** 이 함수는 `_clients`에서 삭제하지 않음!
- 루프 중간에 맵 수정 방지
- cleanup 단계에서 일괄 처리

#### 2.5 cleanupMarkedClients()

```cpp
void IrcServer::cleanupMarkedClients() {
	std::vector<struct pollfd>::iterator it = _fds.begin();
	while (it != _fds.end()) {
		if (it->fd == -1) {
			// _clients에서 찾기
			// 주의: 이미 removeClientFromServer()에서 fd가 -1로 변경됨
			// → _clients.find(-1)은 실패함
			
			// 대신, 추적해야 할 fd를 별도로 저장해야 함
			it = _fds.erase(it);
		} else {
			++it;
		}
	}
}
```

**문제 발견:** 현재 설계에서는 fd를 -1로 변경하면 
원래 fd 값을 잃어버립니다!

**해결책:**
```cpp
// 더 나은 방식: cleanup 콜렉션 유지
private:
	std::set<int> _clientsToRemove;  // Server.hpp에 추가

// markClientForRemoval()에서:
void IrcServer::markClientForRemoval(int fd) {
	_clientsToRemove.insert(fd);
	// fd는 유지하되 폴에서만 -1로 표시
	for (size_t i = 0; i < _fds.size(); ++i) {
		if (_fds[i].fd == fd) {
			_fds[i].fd = -1;
			break;
		}
	}
}

// cleanupMarkedClients()에서:
void IrcServer::cleanupMarkedClients() {
	for (std::set<int>::iterator it = _clientsToRemove.begin(); 
	     it != _clientsToRemove.end(); ++it) {
		int fd = *it;
		Client* client = getClient(fd);
		if (client) {
			removeClientFromServer(client);
		}
	}
	_clientsToRemove.clear();
	
	// 그 다음 _fds에서 -1 항목 제거
	for (std::vector<struct pollfd>::iterator it = _fds.begin(); 
	     it != _fds.end(); ) {
		if (it->fd == -1) {
			it = _fds.erase(it);
		} else {
			++it;
		}
	}
}
```

#### 2.6 shouldExitServer()

```cpp
bool IrcServer::shouldExitServer() {
	// 현재는 항상 false (무한 루프)
	// 나중에 신호 핸들러와 연동 가능
	return false;
}
```

#### 2.7 canRecover()

```cpp
bool IrcServer::canRecover(const ServerException& e) {
	std::string msg = e.what();
	
	// 서버 자체의 복구 불가능한 에러
	if (msg.find("socket") != std::string::npos ||
	    msg.find("bind") != std::string::npos ||
	    msg.find("listen") != std::string::npos) {
		return false;
	}
	
	// 클라이언트 관련 에러는 복구 가능
	return true;
}
```

---

## Phase 4: run() 메서드 리팩토링

### Step 3: 기존 run() 대체

**기존 run() 메서드를 완전히 다시 작성:**

```cpp
void IrcServer::run() {
	while (!shouldExitServer()) {
		try {
			checkPingTimeOut();

			int timeout_ms = 1000;
			int poll_result = poll(&_fds[0], _fds.size(), timeout_ms);
			
			if (poll_result < 0) {
				if (errno == EINTR) {
					continue;  // 신호로 인한 중단
				}
				throw ServerException(ERR_POLL);
			}

			// 이벤트 처리
			for (size_t i = 0; i < _fds.size(); ++i) {
				if (_fds[i].revents == 0) {
					continue;  // 이벤트 없음
				}

				// 서버 소켓 이벤트
				if (_fds[i].fd == this->_fd) {
					if (_fds[i].revents & POLLIN) {
						processServerEvent();
					}
					continue;
				}

				int client_fd = _fds[i].fd;

				// 클라이언트 읽기
				if (_fds[i].revents & POLLIN) {
					if (!processClientRead(client_fd)) {
						markClientForRemoval(client_fd);
						continue;
					}
				}

				// 클라이언트 쓰기 (fd 재확인)
				if (_fds[i].fd != -1 && (_fds[i].revents & POLLOUT)) {
					if (!processClientWrite(_fds[i].fd)) {
						markClientForRemoval(_fds[i].fd);
					}
				}
			}

			// 표시된 클라이언트들 정리
			cleanupMarkedClients();

		} catch (const ServerException& e) {
			serverLog(this->_fd, LOG_ERR, C_ERR, e.what());
			if (!canRecover(e)) {
				break;  // 종료
			}
		}
	}
}
```

**주요 변화:**
1. `while (!shouldExitServer())` - 명확한 조건
2. 깊은 중첩 제거
3. 명시적 에러 체크
4. 중앙화된 cleanup

---

## Phase 5: 테스트

### 단위 테스트 작성

```cpp
// test/test_refactored.py
def test_process_server_event():
    """새 클라이언트 연결이 올바르게 처리되는가"""
    pass

def test_process_client_read():
    """클라이언트 읽기가 실패하면 false 반환하는가"""
    pass

def test_cleanup_marked_clients():
    """표시된 클라이언트가 정리되는가"""
    pass
```

### 기존 테스트 재실행

```bash
make test
```

---

## Phase 6: 추가 개선 (Optional)

### 향후 리팩토링 아이디어

1. **루프 순회 방향 개선**
   ```cpp
   // 현재: 앞에서부터 순회 (이미 개선됨)
   // 문제 없음
   ```

2. **이벤트 핸들러 객체화**
   ```cpp
   class EventHandler {
       virtual bool handle(int fd) = 0;
   };
   
   class ServerEventHandler : public EventHandler { ... };
   class ClientReadHandler : public EventHandler { ... };
   ```

3. **상태 머신 도입**
   ```cpp
   enum ServerState {
       RUNNING,
       SHUTTING_DOWN,
       STOPPED
   };
   ```

---

## 체크리스트

- [ ] `Server.hpp`에 새 함수 선언 추가
- [ ] `Server.cpp`에 새 함수 구현
- [ ] `_clientsToRemove` 멤버 변수 추가 (선택사항이지만 권장)
- [ ] `run()` 메서드 전체 리팩토링
- [ ] 컴파일 테스트
- [ ] 기존 테스트 실행 (`make test`)
- [ ] 새 테스트 케이스 작성
- [ ] 코드 리뷰

---

## FAQ

**Q: 왜 markClientForRemoval()과 cleanupMarkedClients()로 분리?**
A: 루프 중간에 _clients 맵을 수정하면 이터레이터 무효화 위험. 
   분리하면 루프가 완전히 끝난 후 한 번에 정리 가능.

**Q: processServerEvent()에서 에러를 로깅하는데, acceptClient()도 이미 로깅하지 않나?**
A: 맞습니다. 중복 제거 가능. 또는 더 상세한 컨텍스트를 로깅할 수 있습니다.

**Q: bool 반환 대신 예외를 던지지 않은 이유?**
A: 예외는 "예외적" 상황용. 클라이언트 삭제는 정상적인 상황입니다.

**Q: 기존 handleSocketRead/Write() 함수는 삭제?**
A: 기존 함수는 유지. 새 함수는 다른 용도로 사용하거나 리팩토링 단계에서 제거 가능합니다.

---

## 구현 시간 예상

- 함수 선언: 5분
- 함수 구현: 30분
- run() 리팩토링: 15분
- 컴파일 및 테스트: 20분
- 총: ~70분

# 실제 적용 매뉴얼

## 🎯 목표
IRC 서버의 main loop (`run()` 메서드)를 리팩토링하여:
- 가독성 향상
- 유지보수성 개선
- 버그 위험 감소

---

## 📋 Step-by-Step 적용

### STEP 1️⃣: Server.hpp 수정 (5분)

**파일:** `include/Server.hpp`

**위치:** Private 섹션에 기존 함수들 아래에 추가

```cpp
private:
	/* ===== 새로 추가할 부분 ===== */
	
	/* Event handling - 이벤트 처리를 작은 함수로 분리 */
	bool									processServerEvent();  // 새 연결 수락
	bool									processClientRead(int fd);  // 클라이언트 데이터 읽기
	bool									processClientWrite(int fd);  // 클라이언트 데이터 쓰기
	
	/* Cleanup - 일관된 cleanup 관리 */
	void									markClientForRemoval(int fd);  // fd 표시만
	void									cleanupMarkedClients();  // 일괄 정리
	
	/* Lifecycle - 서버 종료 관리 */
	bool									shouldExitServer();  // 종료 조건
	bool									canRecover(const ServerException& e);  // 복구 가능 판단
	
	/* State tracking - 제거할 클라이언트 추적 */
	std::set<int>							_clientsToRemove;  // 제거 대상 fd 목록
	
	/* ===== 끝 ===== */
```

**컴파일 테스트:**
```bash
make clean
make
# 에러가 없어야 함
```

---

### STEP 2️⃣: 새 함수 구현 (30분)

**파일:** `src/Server.cpp`

**어디에 추가?** 기존 `run()` 메서드 이전

#### 2.1) processServerEvent() - 새 클라이언트 연결

```cpp
bool IrcServer::processServerEvent() {
	try {
		acceptClient();
		return true;
	} catch (const ServerException& e) {
		// 연결 수락 실패 로깅 (acceptClient()도 로깅하지만 추가 컨텍스트)
		return false;
	}
}
```

**여기서 주의:**
- `acceptClient()`는 이미 내부에서 로깅함
- 예외는 여기서 catch하지만 심각한 에러가 아님
- 호출자에게 실패를 알리기만 함

#### 2.2) processClientRead(int fd) - 클라이언트 읽기

```cpp
bool IrcServer::processClientRead(int fd) {
	Client* client = getClient(fd);
	if (!client) {
		return false;  // 클라이언트가 없음
	}

	char buffer[BUFFER_SIZE];
	size_t recvLen = recv(fd, buffer, BUFFER_SIZE - 1, 0);
	
	// 연결 종료 또는 읽기 에러
	if (recvLen <= 0) {
		return false;  // 클라이언트 제거 필요
	}

	buffer[recvLen] = '\0';
	client->appendToRecvBuffer(buffer);
	
	// 메시지 추출 및 처리
	std::string str;
	while (getClient(fd) && client->extractMessage(str)) {
		Cmd cmdHandler(*this, str, fd);
		serverLog(fd, LOG_INPUT, C_MSG, str);
		
		// handleClientCmd() 실패 = 클라이언트 삭제됨
		if (!cmdHandler.handleClientCmd()) {
			return false;
		}
	}
	
	return true;  // 정상 처리
}
```

**체크포인트:**
- [ ] 기존 `handleSocketRead()`와 동일한 로직인지 확인
- [ ] `extractMessage()` 루프가 동일한가?
- [ ] 로깅이 일치하는가?

#### 2.3) processClientWrite(int fd) - 클라이언트 쓰기

```cpp
bool IrcServer::processClientWrite(int fd) {
	Client* client = getClient(fd);
	if (!client) {
		return false;
	}

	std::string& sendBuffer = client->getSendBuffer();

	if (!sendBuffer.empty()) {
		ssize_t sendLen = send(fd, sendBuffer.c_str(), sendBuffer.size(), 0);
		
		if (sendLen > 0) {
			sendBuffer.erase(0, sendLen);
		} else if (sendLen == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
			// 복구 불가능한 에러
			removeClientFromServer(client);
			return false;
		}
		// EAGAIN/EWOULDBLOCK은 무시
	}

	// 전송 완료 시 POLLOUT 비활성화
	if (sendBuffer.empty()) {
		for (size_t i = 0; i < _fds.size(); ++i) {
			if (_fds[i].fd == fd) {
				_fds[i].events &= ~POLLOUT;
				break;
			}
		}
	}

	return true;
}
```

#### 2.4) markClientForRemoval(int fd) - 제거 표시

```cpp
void IrcServer::markClientForRemoval(int fd) {
	// 1. 추적 목록에 추가
	_clientsToRemove.insert(fd);
	
	// 2. Poll에서 무효화 (루프 중간에 인덱스 변경 없음)
	for (size_t i = 0; i < _fds.size(); ++i) {
		if (_fds[i].fd == fd) {
			_fds[i].fd = -1;
			break;
		}
	}
}
```

**중요:**
- `_clientsToRemove` 셋에 추가 (fd 보존)
- Poll에서만 -1로 표시 (인덱스 안전성)

#### 2.5) cleanupMarkedClients() - 일괄 정리

```cpp
void IrcServer::cleanupMarkedClients() {
	// 1. _clientsToRemove의 fd들을 _clients에서 삭제
	for (std::set<int>::iterator it = _clientsToRemove.begin();
	     it != _clientsToRemove.end(); ++it) {
		int fd = *it;
		Client* client = getClient(fd);
		if (client) {
			removeClientFromServer(client);
		}
	}
	_clientsToRemove.clear();  // 초기화
	
	// 2. Poll에서 -1인 항목 제거
	for (std::vector<struct pollfd>::iterator it = _fds.begin();
	     it != _fds.end(); ) {
		if (it->fd == -1) {
			it = _fds.erase(it);
		} else {
			++it;
		}
	}
}
```

#### 2.6) shouldExitServer() - 종료 조건

```cpp
bool IrcServer::shouldExitServer() {
	// 현재는 신호 핸들러로만 종료 (signalHandler() 참고)
	// 무한 루프를 유지
	return false;
}
```

#### 2.7) canRecover() - 복구 판단

```cpp
bool IrcServer::canRecover(const ServerException& e) {
	std::string msg = e.what();
	
	// 복구 불가능: 서버 자체의 문제
	if (msg.find("socket") != std::string::npos ||
	    msg.find("bind") != std::string::npos ||
	    msg.find("listen") != std::string::npos) {
		return false;
	}
	
	// 복구 가능: 클라이언트 관련 문제
	return true;
}
```

**컴파일 테스트:**
```bash
make clean
make
# 에러/경고 확인
```

---

### STEP 3️⃣: run() 메서드 리팩토링 (15분)

**파일:** `src/Server.cpp`

**위치:** 기존 `run()` 메서드 전체 교체

**먼저 백업:**
```bash
cp src/Server.cpp src/Server.cpp.backup
```

**새로운 run():**

```cpp
void IrcServer::run() {
	while (!shouldExitServer()) {
		try {
			// 1. Ping timeout 체크
			checkPingTimeOut();

			// 2. Poll 대기
			int timeout_ms = 1000;
			int poll_result = poll(&_fds[0], _fds.size(), timeout_ms);
			
			if (poll_result < 0) {
				if (errno == EINTR) {
					continue;  // 신호로 인한 중단 → 무시
				}
				throw ServerException(ERR_POLL);
			}

			// 3. 이벤트 처리 (깔끔한 루프)
			for (size_t i = 0; i < _fds.size(); ++i) {
				// 3-1. 이벤트 없으면 스킵
				if (_fds[i].revents == 0) {
					continue;
				}

				// 3-2. 서버 소켓 (새 연결)
				if (_fds[i].fd == this->_fd) {
					if (_fds[i].revents & POLLIN) {
						if (!processServerEvent()) {
							// 연결 수락 실패는 로깅하고 계속
						}
					}
					continue;
				}

				int client_fd = _fds[i].fd;

				// 3-3. 클라이언트 읽기
				if (_fds[i].revents & POLLIN) {
					if (!processClientRead(client_fd)) {
						// 클라이언트 삭제 필요
						markClientForRemoval(client_fd);
						continue;
					}
				}

				// 3-4. 클라이언트 쓰기 (fd 재확인)
				// 읽기에서 클라이언트가 삭제되었을 수 있으므로
				// _fds[i].fd != -1 체크
				if (_fds[i].fd != -1 && (_fds[i].revents & POLLOUT)) {
					if (!processClientWrite(_fds[i].fd)) {
						markClientForRemoval(_fds[i].fd);
					}
				}
			}

			// 4. 정리 (루프 완료 후 한 번만)
			cleanupMarkedClients();

		} catch (const ServerException& e) {
			serverLog(this->_fd, LOG_ERR, C_ERR, e.what());
			
			// 복구 가능한 에러인가?
			if (!canRecover(e)) {
				// 복구 불가능 → 종료
				break;
			}
			// 복구 가능 → 계속 진행
		}
	}
}
```

**비교: 이전 vs 이후**

| 항목 | 이전 | 이후 |
|------|------|------|
| 줄 수 | ~150 | ~100 |
| 중첩 깊이 | 3-4단계 | 1-2단계 |
| 함수 분리 | 없음 | 3개 |
| Bool 반환 | 없음 | 명시적 |
| Cleanup | 분산 | 중앙화 |

**컴파일:**
```bash
make clean
make
```

**에러 확인:**
- Compilation error? → 함수 선언/구현 재확인
- Warning? → 변수 사용 확인

---

### STEP 4️⃣: 테스트 (20분)

#### 4.1 컴파일 테스트
```bash
make
# 성공 여부 확인
```

#### 4.2 기본 기능 테스트
```bash
make test
# test/test_basic.py 실행
```

**예상 결과:**
```
--- Server Response ---
:ircserv 001 testbot ...
✅ PASS: Registration successful.
✅ PASS: PING/PONG successful.
```

#### 4.3 로그 확인
```bash
./ircserv 6667 password
# 여러 클라이언트로 테스트

# 로그에서 확인할 사항:
# - Server started
# - Client connected
# - Commands received
# - Client disconnected
# - No errors
```

#### 4.4 메모리 누수 테스트 (선택)
```bash
valgrind --leak-check=full ./ircserv 6667 password
```

---

### STEP 5️⃣: 확인 체크리스트

- [ ] 코드 컴파일 성공
- [ ] 기존 테스트 통과 (`make test`)
- [ ] 여러 클라이언트 동시 연결 가능
- [ ] 클라이언트 정상 종료 가능
- [ ] SIGINT (Ctrl+C) 처리 정상
- [ ] 메모리 누수 없음
- [ ] 로그 출력 정상
- [ ] Git diff 검토 완료

---

## 🐛 문제 해결

### 컴파일 에러: "선언되지 않은 함수"

**원인:** 함수 선언을 Server.hpp에 추가하지 않음

**해결:**
```cpp
// include/Server.hpp에서 private 섹션 확인
// bool processServerEvent(); 등이 있는지 확인
```

### 런타임 에러: "Segmentation fault"

**원인:** 
- `_clientsToRemove` 초기화 안 됨
- fd 참조 오류

**해결:**
```cpp
// Server.hpp에서 멤버 변수 확인
std::set<int> _clientsToRemove;  // 있는가?

// Server.cpp의 생성자/소멸자 확인
// 필요시 수동 초기화
```

### 테스트 실패: "RPL_WELCOME not found"

**원인:** 기본 기능 손상

**해결:**
```bash
# 기존 코드 복원 후 재시도
cp src/Server.cpp.backup src/Server.cpp

# 천천히 다시 리팩토링
```

---

## 📊 예상 성과

**리팩토링 후:**

| 메트릭 | 개선 |
|-------|------|
| 가독성 | ⬆️⬆️⬆️ (30% 향상) |
| 유지보수성 | ⬆️⬆️⬆️ (테스트 용이) |
| 버그 위험 | ⬇️⬇️ (명확한 로직) |
| 코드 라인 수 | ⬇️ (~20% 감소) |
| 복잡도 (cyclomatic) | ⬇️⬇️ |

---

## 💡 다음 단계

리팩토링이 완료되면 다음을 고려:

1. **추가 분리 검토**
   - `removeClientFromServer()` 분리 가능?
   - 채널 관리 로직 분리?

2. **더 나은 에러 처리**
   - Exception 타입 세분화
   - 에러 복구 정책 수립

3. **성능 최적화**
   - Poll 방식 재검토 (epoll, kqueue)
   - 메시지 버퍼 최적화

4. **테스트 확대**
   - 스트레스 테스트 (많은 클라이언트)
   - 엣지 케이스 테스트

---

## 📞 문제 발생 시

1. 기존 코드와 비교 (git diff)
2. 로그 메시지 확인
3. 작은 부분부터 되돌리기
4. 다시 시도

**성공을 기원합니다! 🎉**