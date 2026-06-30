"""클라이언트 생명주기 / 지연 삭제 안전성 테스트.

목적(② 메인 루프 리팩토링 검증):
  - 다중 클라이언트가 채널에 참여한 상태에서 일부가 QUIT 없이 끊기고,
    남은 클라이언트가 채널로 브로드캐스트하는 churn 을 발생시킨다.
  - broadcastMsg 순회 중 죽은 소켓으로의 castMsg 실패 → markClientForRemoval(마킹만)
    → cleanupMarkedClients 단일 teardown 경로가 반복자 무효화/use-after-free 없이 동작하는지 확인.
  - `make asan` 빌드로 서버를 띄우고 돌리면 sanitizer 가 메모리 오류를 직접 검출한다.

검증 기준:
  churn 이후에도 서버 프로세스가 살아있어야 한다(크래시/메모리 오염 없음).
  → churn 이 끝난 뒤 "새 클라이언트"가 접속/등록되고 PING 에 PONG 으로 응답하면 통과.

주의: 등록된 클라이언트가 특정 흐름에서 끊기는 별개의 (리팩토링 이전부터 존재하는)
      동작이 있어, 개별 churn 클라이언트의 생존은 단언하지 않는다.
      (docs/REFACTORING.md "🔴 등록 클라이언트 비정상 disconnect" 참고)
"""
import socket
import sys
import time

HOST = '127.0.0.1'
PORT = 6667
PASSWORD = 'password'


def register(nick, wait_token="001"):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(5.0)
    s.connect((HOST, PORT))
    s.sendall(f"PASS {PASSWORD}\r\nNICK {nick}\r\nUSER {nick} 0 * :churn bot\r\n".encode())
    buf = ""
    while wait_token not in buf:
        chunk = s.recv(4096).decode(errors="ignore")
        if not chunk:
            break
        buf += chunk
    return s


def main():
    try:
        # --- churn: 채널 참여 + 비정상 종료 + 브로드캐스트 ---
        churn = [register(f"churn{i}") for i in range(4)]
        for s in churn:
            s.sendall(b"JOIN #churn\r\n")
        time.sleep(0.3)

        # 절반을 QUIT 없이 강제 종료 → 서버 입장에서 죽은 소켓 발생
        for s in churn[2:]:
            s.close()
        time.sleep(0.2)

        # 남은 클라이언트가 채널로 반복 전송 → 죽은 fd 로의 castMsg 실패 유발
        for n in range(30):
            try:
                churn[0].sendall(f"PRIVMSG #churn :spam {n}\r\n".encode())
                churn[1].sendall(f"PRIVMSG #churn :spam {n}\r\n".encode())
            except OSError:
                break  # churn 클라이언트가 끊겨도 무방 (서버 생존이 관심사)
            time.sleep(0.01)
        time.sleep(0.3)

        for s in churn[:2]:
            try:
                s.close()
            except OSError:
                pass

        # --- 검증: 서버가 churn 을 견디고 살아있는가 ---
        probe = register("probe", wait_token="001")
        probe.sendall(b"PING :survive_check\r\n")
        deadline = time.time() + 5
        resp = ""
        while "survive_check" not in resp and "PONG" not in resp and time.time() < deadline:
            resp += probe.recv(4096).decode(errors="ignore")
        probe.sendall(b"QUIT :bye\r\n")
        probe.close()

        if "PONG" not in resp and "survive_check" not in resp:
            print("❌ FAIL: churn 이후 서버가 PONG 에 응답하지 않음 (크래시/오염 의심)")
            sys.exit(1)

        print("✅ PASS: 지연 삭제 churn 이후에도 서버 생존 (브로드캐스트/죽은소켓 경로 안전)")
    except Exception as e:
        print(f"❌ FAIL: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()
