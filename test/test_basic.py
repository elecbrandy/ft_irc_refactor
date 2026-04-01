import socket
import time
import sys

# 서버 설정 정보
HOST = '127.0.0.1'
PORT = 6667
PASSWORD = 'password'

def test_connection_and_registration():
    try:
        # 1. 소켓 연결
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(3.0) # 타임아웃 3초 설정
        sock.connect((HOST, PORT))
        
        # 2. 인증 및 등록 메시지 전송 (CRLF 필수)
        sock.sendall(f"PASS {PASSWORD}\r\n".encode('utf-8'))
        sock.sendall(b"NICK testbot\r\n")
        sock.sendall(b"USER testbot 0 * :Test Bot Realname\r\n")
        
        # 3. 서버 응답 확인 (RPL_WELCOME 001 코드가 오는지 확인)
        response = sock.recv(4096).decode('utf-8')
        print("--- Server Response ---")
        print(response)
        
        if "001 testbot" not in response:
            print("❌ FAIL: RPL_WELCOME (001) not found.")
            sys.exit(1)
        else:
            print("✅ PASS: Registration successful.")

        # 4. PING / PONG 테스트
        sock.sendall(b"PING :testping\r\n")
        response = sock.recv(1024).decode('utf-8')
        
        if "PONG :testping" not in response:
            print("❌ FAIL: PONG not received.")
            sys.exit(1)
        else:
            print("✅ PASS: PING/PONG successful.")
            
        # 5. 종료
        sock.sendall(b"QUIT :bye\r\n")
        sock.close()
        
    except Exception as e:
        print(f"❌ FAIL: Test thrown an exception: {e}")
        sys.exit(1)

if __name__ == "__main__":
    time.sleep(1) # 서버가 완전히 뜰 때까지 약간 대기
    test_connection_and_registration()