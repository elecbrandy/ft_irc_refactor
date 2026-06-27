#!/usr/bin/env python3
"""테스트 러너: test/ 하위의 test_*.py 를 모두 실행하고 결과를 종합한다.

각 테스트 파일은 독립 프로세스로 실행되며, exit code 0 이면 통과로 본다.
하나라도 실패하면 러너도 non-zero 로 종료 → make test / CI 연동 가능.
"""
import glob
import os
import subprocess
import sys

TEST_DIR = os.path.dirname(os.path.abspath(__file__))


def main():
    files = sorted(glob.glob(os.path.join(TEST_DIR, "test_*.py")))
    if not files:
        print("실행할 테스트가 없습니다 (test/test_*.py).")
        return 0

    results = []
    for path in files:
        name = os.path.basename(path)
        print(f"\n===== RUN {name} =====")
        code = subprocess.call([sys.executable, path])
        results.append((name, code))
        print(f"===== {'PASS' if code == 0 else 'FAIL'} {name} (exit {code}) =====")

    print("\n----- 요약 -----")
    failed = 0
    for name, code in results:
        mark = "✅" if code == 0 else "❌"
        print(f"{mark} {name}")
        if code != 0:
            failed += 1
    print(f"통과 {len(results) - failed}/{len(results)}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
