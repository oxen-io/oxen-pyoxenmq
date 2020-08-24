import pylokimq
import subprocess
import shlex
import argparse
import time

def handle_auth(args, cmd):
    result = subprocess.run(args=cmd, check=False)
    if result.returncode == 0:
        return "OK"
    else:
        return "REJECT"

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bind", required=True, help="url to bind auth socket to")
    ap.add_argument("--cmd", required=True, help="script to call for authentication")
    args = ap.parse_args()
    cmd = shlex.split(args.cmd)
    lmq = pylokimq.LokiMQ()
    lmq.listen_plain(args.bind)
    lmq.add_anonymous_category("llarp")
    lmq.add_request_command("llarp", "auth", lambda x : handle_auth(x, cmd))
    lmq.start()
    print("server started")
    while True:
        time.sleep(1)

if __name__ == '__main__':
    main()

