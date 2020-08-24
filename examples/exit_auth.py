import pylokimq
import base64
import subprocess
import shlex
import argparse
import io
import time

def decode_str(data):
    s = ''
    l = ''
    while True:
        ch = data.read(1)
        if ch == b':':
            return data.read(int(l))
        else:
            l += chr(ch)

def decode_list(data):
    l = []
    ch = data.read(1)
    assert ch == b'l'
    while True:
        ch = data.peek(1)
        if ch == b'e':
            data.read(1)
            return l
        l += decode_value(data)

def decode_dict(data):
    d = dict()
    ch = data.read(1)
    assert ch == b'd'
    while True:
        ch = data.peek(1)
        if ch == b'e':
            data.read(1)
            return d
        k = decode_str(data)
        v = decode_value(data)
        d[k] = v

def decode_int(data):
    ch = data.read(1)
    i = ''
    assert ch == b'i'
    while True:
        ch = data.read(1)
        if ch == b'e':
            return int(i)
        i += chr(ch)

def decode_value(data):
    ch = data.peek(1)
    if ch in b'0123456789':
        return decode_string(data)
    if ch == b'i':
        return decode_int(data)
    if ch == b'd':
        return decode_dict(data)
    if ch == b'l':
        return decode_list(data)
    raise Exception("invalid char: {}".format(ch))


def decode_address(data):
    return '{}.loki'.format(pylokimq.base32z_encode(decode_dict(data)[b's'][b's']))

def handle_auth(args, cmd):
    cmd += decode_address(io.BytesIO(args[0]))
    cmd += base64.b64encode(args[1]).decode('ascii')
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

