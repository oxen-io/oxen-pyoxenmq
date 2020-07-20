import pylokimq
import time

def handle_auth(args):
    print(args)
    return "OK"

lmq = pylokimq.LokiMQ()
lmq.listen_plain("ipc:///tmp/lmq.sock")
lmq.add_anonymous_category("llarp")
lmq.add_request_command("llarp", "authexit", handle_auth)
lmq.start()

while True:
    time.sleep(1)
