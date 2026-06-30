"""등록(USER) 검증 테스트.

checkRealname / checkUsername 표준화 회귀 테스트:
  - 표준 IRC realname 은 숫자/기호를 포함할 수 있어야 하고,
  - username 도 alnum 외 문자를 허용해야 한다.
이전에는 realname 에 숫자가 있으면 등록 단계에서 끊겼다(= 과거 "churn disconnect" 오인의 원인).
"""
import socket
import sys
import time

HOST = '127.0.0.1'
PORT = 6667
PASSWORD = 'password'


def try_register(nick, user, realname, timeout=4.0):
    """주어진 USER 파라미터로 등록 시도. 등록 성공(001 수신) 여부를 반환."""
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(timeout)
    try:
        s.connect((HOST, PORT))
        s.sendall(f"PASS {PASSWORD}\r\nNICK {nick}\r\nUSER {user} 0 * :{realname}\r\n".encode())
        buf = ""
        while "001" not in buf:
            chunk = s.recv(4096).decode(errors="ignore")
            if not chunk:
                break  # 서버가 연결을 끊음 → 등록 실패
            buf += chunk
        return "001" in buf
    except OSError:
        return False
    finally:
        try:
            s.close()
        except OSError:
            pass


def main():
    failures = []

    # 1) realname 에 숫자/기호 — 등록되어야 함 (핵심 회귀 케이스)
    if not try_register("regA", "regA", "WeeChat 3.0"):
        failures.append("realname with digits ('WeeChat 3.0') was rejected")

    # 2) realname 에 구두점/공백 — 등록되어야 함
    if not try_register("regB", "regB", "John Q. Public-42"):
        failures.append("realname with punctuation was rejected")

    # 3) username 에 alnum 외 문자(밑줄/점) — 등록되어야 함
    if not try_register("regC", "guest_1.user", "real name"):
        failures.append("username with '_'/'.' was rejected")

    # 4) realname 에 제어문자(\x01) — 거부되어야 함 (등록 실패가 정상)
    if try_register("regD", "regD", "bad\x01name"):
        failures.append("realname with control char was accepted (should be rejected)")

    if failures:
        for f in failures:
            print(f"❌ FAIL: {f}")
        sys.exit(1)
    print("✅ PASS: realname/username 표준 문자 허용 + 제어문자 거부 확인")


if __name__ == "__main__":
    time.sleep(0.2)
    main()
