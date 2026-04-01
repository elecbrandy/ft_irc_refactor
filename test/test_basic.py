import socket
import time
import sys

HOST = '127.0.0.1'
PORT = 6667
PASSWORD = 'password'

def test_connection_and_registration():
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5.0)
        sock.connect((HOST, PORT))
        print(f"✅ Connected to {HOST}:{PORT}")
        
        sock.sendall(f"PASS {PASSWORD}\r\n".encode('utf-8'))
        sock.sendall(b"NICK testbot\r\n")
        sock.sendall(b"USER testbot 0 * :Test Bot Realname\r\n")
        
        # 3. Welcome 메시지 및 MOTD 끝(376)까지 대기
        print("--- Waiting for Registration & MOTD (001~376) ---")
        full_response = ""
        while True:
            chunk = sock.recv(4096).decode('utf-8')
            if not chunk:
                break
            full_response += chunk
            # 376(End of MOTD)이 오면 서버가 초기 메시지를 다 보낸 것으로 간주
            if "376 testbot" in full_response:
                break
        
        print(full_response.strip())
        print("✅ PASS: Registration and MOTD received.")

        # 4. PING / PONG 테스트
        # 버퍼가 완전히 비워졌는지 확인하기 위해 아주 짧게 대기
        time.sleep(0.1)
        
        print("\n--- Testing PING/PONG ---")
        test_payload = "testping_12345"
        sock.sendall(f"PING :{test_payload}\r\n".encode('utf-8'))
        
        # 이제 순수하게 PING에 대한 응답만 들어옵니다.
        response = sock.recv(1024).decode('utf-8')
        print(f"Received from server: {response.strip()}")
        
        if "PONG" in response.upper() and test_payload in response:
            print("✅ PASS: PING/PONG successful.")
        else:
            print(f"❌ FAIL: Expected PONG but got something else.")
            sys.exit(1)
            
        sock.sendall(b"QUIT :Testing finished\r\n")
        sock.close()
        print("\n✅ All tests passed!")
        
    except Exception as e:
        print(f"❌ FAIL: {e}")
        sys.exit(1)

if __name__ == "__main__":
    time.sleep(2)
    test_connection_and_registration()
