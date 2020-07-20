import pylokimq

def do_connected(lmq, conn):
    print("connected via", conn)
    return lmq.request(conn, "exit.auth", "dq3j4dj99w6wi4t4yjnya8sxtqr1rojt8jgnn6467o6aoenm3o3o.loki")

def do_request(lmq):
    print('connect')
    conn = lmq.connect_remote("ipc:///tmp/lokinet-exit.sock")
    if conn:
        return do_connected(lmq, conn)

def main():
    lmq = pylokimq.LokiMQ()
    print("start")
    lmq.start()
    print(do_request(lmq))
    print("done")

main()
