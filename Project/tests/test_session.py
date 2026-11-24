#!/usr/bin/env python3
"""Integration test: truyền dòng, tài khoản và phiên.

Chạy:  python3 -I tests/test_session.py ./server
Server được chạy trong thư mục tạm với cấu hình riêng (port ngẫu nhiên,
PBKDF2 ít vòng để test nhanh), nên không đụng tới data/ và logs/ thật.
"""
import json
import os
import re
import signal
import socket
import sqlite3
import subprocess
import sys
import tempfile
import time

SERVER = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "./server")
TMP = tempfile.mkdtemp(prefix="phidoi-test-")
CONF = os.path.join(TMP, "test.conf")
PORT_FILE = os.path.join(TMP, "port")
DB = os.path.join(TMP, "game.db")
LOG = os.path.join(TMP, "server.log")

with open(CONF, "w") as f:
    f.write("bind_address = 127.0.0.1\n"
            "db_path = %s\nlog_path = %s\n"
            "pbkdf2_iterations = 1000\n"
            "send_buffer_max = 65536\n" % (DB, LOG))

passed = 0
failed = 0


def check(name, cond, detail=""):
    global passed, failed
    if cond:
        passed += 1
        print("  ok   %s" % name)
    else:
        failed += 1
        print("  FAIL %s %s" % (name, detail))


class Client:
    def __init__(self, port):
        self.s = socket.create_connection(("127.0.0.1", port), timeout=5)
        self.buf = b""

    def send_raw(self, data):
        self.s.sendall(data)

    def send(self, obj):
        self.send_raw((json.dumps(obj, separators=(",", ":")) + "\r\n").encode())

    def recv(self):
        while b"\r\n" not in self.buf:
            chunk = self.s.recv(65536)
            if not chunk:
                raise ConnectionError("closed")
            self.buf += chunk
        line, self.buf = self.buf.split(b"\r\n", 1)
        return json.loads(line.decode())

    def req(self, obj):
        self.send(obj)
        return self.recv()

    def is_closed(self, timeout=3):
        self.s.settimeout(timeout)
        try:
            while True:
                chunk = self.s.recv(65536)
                if not chunk:
                    return True
                self.buf += chunk
        except ConnectionResetError:
            return True                # RST cũng là đã đóng
        except socket.timeout:
            return False
        finally:
            self.s.settimeout(5)

    def close(self):
        self.s.close()


def start_server():
    if os.path.exists(PORT_FILE):
        os.remove(PORT_FILE)
    p = subprocess.Popen([SERVER, "-c", CONF, "-p", "0", "--port-file", PORT_FILE],
                         stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    for _ in range(100):
        if os.path.exists(PORT_FILE) and os.path.getsize(PORT_FILE) > 0:
            with open(PORT_FILE) as f:
                return p, int(f.read())
        if p.poll() is not None:
            raise RuntimeError("server exited: %s" % p.stderr.read().decode())
        time.sleep(0.05)
    p.kill()
    raise RuntimeError("server did not start")


def stop_server(p):
    p.send_signal(signal.SIGTERM)
    return p.wait(timeout=10)


def code(res):
    return res.get("code")


def main():
    # ---------- tạo admin bằng dòng lệnh ----------
    print("create-admin")
    r = subprocess.run([SERVER, "-c", CONF, "--create-admin", "boss"],
                       input=b"Admin123\nAdmin123\n", capture_output=True)
    check("--create-admin exits 0", r.returncode == 0, r.stderr.decode())

    proc, port = start_server()

    # ---------- truyền dòng ----------
    print("framing")
    c = Client(port)
    res = c.req({"cmd": "PING"})
    check("PING -> 1000 + time", code(res) == 1000 and isinstance(res["data"]["time"], int), res)

    c.send_raw(b'{"cmd":"PI')
    time.sleep(0.1)
    c.send_raw(b'NG"}\r\n')
    check("message split across 2 sends", code(c.recv()) == 1000)

    c.send_raw(b'{"cmd":"PING"}\r\n{"cmd":"PING"}\n\r\n')
    check("2 messages in 1 send, \\n alone accepted, blank line ignored",
          code(c.recv()) == 1000 and code(c.recv()) == 1000)

    c.send_raw(b'{"cmd":"PING"\r\n')
    check("bad JSON -> 2001", code(c.recv()) == 2001)
    check("connection kept after 2001", code(c.req({"cmd": "PING"})) == 1000)
    c.send_raw(b'[1,2]\r\n')
    check("JSON not an object -> 2001", code(c.recv()) == 2001)
    check("unknown cmd -> 2002", code(c.req({"cmd": "FLY"})) == 2002)
    check("missing cmd -> 2002", code(c.req({"x": 1})) == 2002)
    check("WHOAMI before login -> 3001", code(c.req({"cmd": "WHOAMI"})) == 3001)
    check("CHANGE_PASSWORD before login (no params) -> 3001",
          code(c.req({"cmd": "CHANGE_PASSWORD"})) == 3001)

    big = Client(port)
    big.send_raw(b"a" * 9000)
    res = big.recv()
    check("message > 8192 bytes -> 2004", code(res) == 2004, res)
    check("connection closed after 2004", big.is_closed())

    # ---------- đăng ký ----------
    print("register")
    check("REGISTER ok", code(c.req({"cmd": "REGISTER", "username": "son", "password": "Abc123"})) == 1000)
    check("REGISTER same name, other case -> 3005",
          code(c.req({"cmd": "REGISTER", "username": "SON", "password": "Abc123"})) == 3005)
    check("REGISTER bad username -> 3006",
          code(c.req({"cmd": "REGISTER", "username": "ab", "password": "Abc123"})) == 3006)
    check("REGISTER bad username chars -> 3006",
          code(c.req({"cmd": "REGISTER", "username": "son tung", "password": "Abc123"})) == 3006)
    check("REGISTER short password -> 3007",
          code(c.req({"cmd": "REGISTER", "username": "huy", "password": "123"})) == 3007)
    check("REGISTER missing password -> 2003",
          code(c.req({"cmd": "REGISTER", "username": "huy"})) == 2003)
    check("REGISTER password not a string -> 2003",
          code(c.req({"cmd": "REGISTER", "username": "huy", "password": 123456})) == 2003)

    a, b = Client(port), Client(port)
    a.send({"cmd": "REGISTER", "username": "race", "password": "Abc123"})
    b.send({"cmd": "REGISTER", "username": "race", "password": "Abc123"})
    codes = sorted([code(a.recv()), code(b.recv())])
    check("2 clients register same name at once -> one 1000, one 3005", codes == [1000, 3005], codes)
    a.close()
    b.close()

    # ---------- đăng nhập ----------
    print("login")
    check("LOGIN wrong password -> 3003",
          code(c.req({"cmd": "LOGIN", "username": "son", "password": "wrong1"})) == 3003)
    check("LOGIN unknown user -> 3003",
          code(c.req({"cmd": "LOGIN", "username": "nobody", "password": "Abc123"})) == 3003)
    check("LOGIN invalid username format -> 3003",
          code(c.req({"cmd": "LOGIN", "username": "a b", "password": "Abc123"})) == 3003)
    check("LOGIN missing params -> 2003", code(c.req({"cmd": "LOGIN"})) == 2003)
    res = c.req({"cmd": "LOGIN", "username": "SON", "password": "Abc123"})
    check("LOGIN ok (case-insensitive name)", code(res) == 1000
          and isinstance(res["data"].get("user_id"), int) and res["data"]["user_id"] > 0
          and {k: v for k, v in res["data"].items() if k != "user_id"} ==
          {"username": "son", "role": "PLAYER", "team_id": None}, res)
    son_id = res["data"].get("user_id")
    check("LOGIN again on same connection -> 3002",
          code(c.req({"cmd": "LOGIN", "username": "son", "password": "Abc123"})) == 3002)
    check("REGISTER while logged in -> 3002",
          code(c.req({"cmd": "REGISTER", "username": "other", "password": "Abc123"})) == 3002)
    res = c.req({"cmd": "WHOAMI"})
    check("WHOAMI", code(res) == 1000 and res["data"]["username"] == "son"
          and res["data"]["user_id"] == son_id and res["data"]["state"] == "IDLE", res)

    # LOGIN và WHOAMI gửi dính liền: response phải đúng thứ tự dù LOGIN chạy trên pool
    d = Client(port)
    d.send_raw(b'{"cmd":"LOGIN","username":"race","password":"Abc123"}\r\n{"cmd":"WHOAMI"}\r\n')
    r1, r2 = d.recv(), d.recv()
    check("pipelined LOGIN then WHOAMI answered in order",
          code(r1) == 1000 and code(r2) == 1000 and r2["data"]["username"] == "race", (r1, r2))
    d.close()

    # ---------- đổi mật khẩu ----------
    print("change password")
    check("CHANGE_PASSWORD wrong old -> 3003",
          code(c.req({"cmd": "CHANGE_PASSWORD", "old_password": "nope00", "new_password": "Xyz789"})) == 3003)
    check("CHANGE_PASSWORD short new -> 3007",
          code(c.req({"cmd": "CHANGE_PASSWORD", "old_password": "Abc123", "new_password": "1"})) == 3007)
    check("CHANGE_PASSWORD ok",
          code(c.req({"cmd": "CHANGE_PASSWORD", "old_password": "Abc123", "new_password": "Xyz789"})) == 1000)
    check("LOGOUT -> 1000", code(c.req({"cmd": "LOGOUT"})) == 1000)
    check("LOGOUT again -> 3001", code(c.req({"cmd": "LOGOUT"})) == 3001)
    check("LOGIN with old password -> 3003",
          code(c.req({"cmd": "LOGIN", "username": "son", "password": "Abc123"})) == 3003)
    check("LOGIN with new password -> 1000",
          code(c.req({"cmd": "LOGIN", "username": "son", "password": "Xyz789"})) == 1000)

    # ---------- thay thế phiên ----------
    print("session replace")
    c2 = Client(port)
    res = c2.req({"cmd": "LOGIN", "username": "son", "password": "Xyz789"})
    check("LOGIN same account elsewhere -> 1000", code(res) == 1000)
    evt = c.recv()
    check("old connection gets FORCE_LOGOUT REPLACED",
          evt.get("type") == "evt" and evt.get("evt") == "FORCE_LOGOUT"
          and evt["data"]["reason"] == "REPLACED", evt)
    check("old connection is closed", c.is_closed())
    check("new connection still works", code(c2.req({"cmd": "WHOAMI"})) == 1000)

    # ---------- admin tạo từ dòng lệnh ----------
    adm = Client(port)
    res = adm.req({"cmd": "LOGIN", "username": "boss", "password": "Admin123"})
    check("admin created by --create-admin can log in", code(res) == 1000
          and res["data"]["role"] == "ADMIN", res)

    # ---------- nhiều kết nối cùng lúc ----------
    print("many connections")
    clients = [Client(port) for _ in range(50)]
    for cl in clients:
        cl.send({"cmd": "PING"})
    check("50 connections PING at once", all(code(cl.recv()) == 1000 for cl in clients))
    for cl in clients:
        cl.close()

    # ---------- tắt server ----------
    print("shutdown")
    rc = stop_server(proc)
    check("server exits 0 on SIGTERM", rc == 0, rc)
    evt = c2.recv()
    check("logged-in client gets FORCE_LOGOUT SHUTDOWN",
          evt.get("evt") == "FORCE_LOGOUT" and evt["data"]["reason"] == "SHUTDOWN", evt)

    # ---------- tài khoản bị khóa ----------
    print("locked account")
    db = sqlite3.connect(DB)
    db.execute("UPDATE users SET locked = 1 WHERE username = 'race'")
    db.commit()
    db.close()
    proc, port = start_server()
    e = Client(port)
    check("users persisted across restart",
          code(e.req({"cmd": "LOGIN", "username": "son", "password": "Xyz789"})) == 1000)
    f2 = Client(port)
    check("locked account, wrong password -> 3003",
          code(f2.req({"cmd": "LOGIN", "username": "race", "password": "wrong1"})) == 3003)
    check("locked account, right password -> 3004",
          code(f2.req({"cmd": "LOGIN", "username": "race", "password": "Abc123"})) == 3004)
    stop_server(proc)

    # ---------- log ----------
    print("log")
    with open(LOG, encoding="utf-8") as fh:
        lines = fh.read().splitlines()
    pattern = re.compile(r"^\[\d\d/\d\d/\d{4} \d\d:\d\d:\d\d\]\$")
    check("every log line starts with [dd/mm/yyyy hh:mm:ss]$",
          lines and all(pattern.match(l) for l in lines))
    text = "\n".join(lines)
    check("no plaintext password in log",
          "Abc123" not in text and "Xyz789" not in text and "Admin123" not in text)
    check("passwords masked as ***", '"password":"***"' in text)
    check("request line format ip:port$user$request$code msg",
          any(re.search(r"\$127\.0\.0\.1:\d+\$son\$\{\"cmd\":\"WHOAMI\"\}\$1000 OK$", l) for l in lines))
    check("PING not logged", '"cmd":"PING"' not in text)
    check("CONNECT/DISCONNECT logged", "$CONNECT" in text and "DISCONNECT closed" in text)
    check("session replace logged", "DISCONNECT replaced" in text)

    print("\n%d passed, %d failed" % (passed, failed))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
