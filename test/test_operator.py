"""채널 운영자(@) / participant-key 리팩토링 회귀 테스트.

participant 맵 key 를 항상 "순수 닉네임"으로 통일하고, 운영자 여부는
_operator 셋으로만 판단하도록 바꾼 변경(@ prefix 제거)에 대한 회귀 그물.

검증 항목:
  1. 채널 첫 참여자는 운영자 → NAMES(353)에 @nick 표시.
  2. 이후 참여자는 일반 → @ 없음.
  3. MODE +o 로 승격하면 NAMES 에 @ 가 붙고, participant 에서 사라지지 않는다.
  4. MODE -o 로 강등하면 @ 가 빠지고, 여전히 채널에 남아 있다.
  5. NICK 변경 후에도 운영자 @ 표시가 유지된다.
  6. 운영자는 KICK 가능, 일반 참여자의 MODE +o 는 482 로 거부된다.

NAMES 전용 명령이 없으므로, 새 클라이언트가 JOIN 할 때 받는 353(RPL_NAMREPLY)
참여자 목록으로 현재 @ 상태를 관찰한다.
"""
import socket
import sys
import time

HOST = '127.0.0.1'
PORT = 6667
PASSWORD = 'password'


def recv_until(sock, needle, timeout=4.0):
    """needle 문자열이 누적 버퍼에 나타날 때까지 수신. 버퍼 전체 반환."""
    sock.settimeout(timeout)
    buf = ""
    try:
        while needle not in buf:
            chunk = sock.recv(4096).decode(errors="ignore")
            if not chunk:
                break
            buf += chunk
    except socket.timeout:
        pass
    return buf


def register(nick):
    """PASS/NICK/USER 등록 완료(001 수신)된 소켓 반환."""
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((HOST, PORT))
    s.sendall(f"PASS {PASSWORD}\r\nNICK {nick}\r\nUSER {nick} 0 * :{nick} real\r\n".encode())
    recv_until(s, "001")
    return s


def join_and_names(sock, chan):
    """채널 JOIN 후 353(NAMES) 라인에서 참여자 목록 문자열을 추출."""
    sock.sendall(f"JOIN {chan}\r\n".encode())
    buf = recv_until(sock, "366")  # 366 = RPL_ENDOFNAMES
    for line in buf.splitlines():
        if " 353 " in line:
            # ... = #chan :@opA usrB
            return line.split(":", 2)[-1].strip()
    return ""


def names_via_fresh_join(chan, probe_nick):
    """새 클라이언트로 채널에 JOIN 해 현재 참여자/@ 상태를 관찰."""
    s = register(probe_nick)
    names = join_and_names(s, chan)
    return s, names


def main():
    failures = []
    socks = []

    try:
        chan = "#opchan"

        # 1) 첫 참여자 opA 는 운영자 → @opA
        a = register("opA")
        socks.append(a)
        names_a = join_and_names(a, chan)
        if "@opA" not in names_a:
            failures.append(f"first joiner not marked operator (NAMES='{names_a}')")

        # 2) 두번째 참여자 usrB 는 일반 → @ 없음, @opA 는 유지
        b = register("usrB")
        socks.append(b)
        names_b = join_and_names(b, chan)
        if "@opA" not in names_b:
            failures.append(f"operator @ lost after 2nd join (NAMES='{names_b}')")
        if "@usrB" in names_b:
            failures.append(f"non-operator wrongly marked @ (NAMES='{names_b}')")
        if "usrB" not in names_b:
            failures.append(f"2nd participant missing from NAMES (NAMES='{names_b}')")

        # 6a) 일반 참여자 usrB 의 MODE +o 는 권한없음(482)으로 거부
        b.sendall(f"MODE {chan} +o opA\r\n".encode())
        resp = recv_until(b, "482")
        if "482" not in resp:
            failures.append("non-operator MODE +o was not rejected with 482")

        # 3) opA 가 usrB 를 +o 승격 → 새 관찰자 시점에 @usrB, 그리고 둘 다 채널에 존재
        a.sendall(f"MODE {chan} +o usrB\r\n".encode())
        recv_until(a, "MODE", timeout=2.0)
        p1, names_p1 = names_via_fresh_join(chan, "probe1")
        socks.append(p1)
        if "@usrB" not in names_p1:
            failures.append(f"+o did not mark usrB as operator (NAMES='{names_p1}')")
        if "@opA" not in names_p1:
            failures.append(f"opA operator @ lost after +o usrB (NAMES='{names_p1}')")
        if names_p1.count("usrB") != 1:
            failures.append(f"usrB duplicated/dropped after +o (NAMES='{names_p1}')")

        # 4) opA 가 usrB 를 -o 강등 → @ 빠지되 여전히 채널에 존재
        a.sendall(f"MODE {chan} -o usrB\r\n".encode())
        recv_until(a, "MODE", timeout=2.0)
        p2, names_p2 = names_via_fresh_join(chan, "probe2")
        socks.append(p2)
        if "@usrB" in names_p2:
            failures.append(f"-o did not remove operator @ from usrB (NAMES='{names_p2}')")
        if "usrB" not in names_p2:
            failures.append(f"usrB dropped from channel after -o (NAMES='{names_p2}')")

        # 5) 운영자 opA 의 NICK 변경 후에도 @ 유지 (participant key 갱신 회귀)
        a.sendall(b"NICK bossA\r\n")
        recv_until(a, "NICK", timeout=2.0)
        p3, names_p3 = names_via_fresh_join(chan, "probe3")
        socks.append(p3)
        if "@bossA" not in names_p3:
            failures.append(f"operator @ lost after NICK change (NAMES='{names_p3}')")
        if "opA" in names_p3:
            failures.append(f"stale old nick remains after NICK change (NAMES='{names_p3}')")

        # 6b) 운영자(bossA)는 KICK 가능 → usrB 강퇴 후 채널에서 사라짐
        a.sendall(f"KICK {chan} usrB :bye\r\n".encode())
        recv_until(a, "KICK", timeout=2.0)
        p4, names_p4 = names_via_fresh_join(chan, "probe4")
        socks.append(p4)
        if "usrB" in names_p4:
            failures.append(f"usrB still present after KICK (NAMES='{names_p4}')")

    finally:
        for s in socks:
            try:
                s.close()
            except OSError:
                pass

    if failures:
        for f in failures:
            print(f"❌ FAIL: {f}")
        sys.exit(1)
    print("✅ PASS: 운영자(@) 표시 / participant-key 순수닉 통일 동작 확인")


if __name__ == "__main__":
    time.sleep(0.2)
    main()
