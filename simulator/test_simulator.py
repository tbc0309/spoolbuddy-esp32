"""Test production LVGL callbacks and WinHTTP against isolated fixtures. No real backend writes."""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import json
import subprocess
import threading

root = Path(__file__).resolve().parent
runtime = root / "runtime"
runtime.mkdir(exist_ok=True)
exe = root / "build/spoolbuddy_sim.exe"
events = []
class Backend(BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass
    def handle_request(self):
        size = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(size)
        events.append((self.command,self.path,self.headers.get("X-API-Key"),body))
        code = 200
        payload = json.dumps({"method":self.command}).encode()
        if self.path == "/api/v1/test/redirect":
            code = 302
            payload = b"{}"
        elif self.path.endswith("/unauthorized"):
            code = 401
            payload = b'{"detail":"test denied"}'
        elif self.path.endswith("/malformed"):
            payload = b'{"invalid"'
        elif self.path.endswith("/oversize"):
            payload = b'{"data":"' + b'x'*70000 + b'"}'
        elif self.path.endswith("/cloud/builtin-filaments"):
            payload = json.dumps([{"filament_id":"GFL05","name":"x"*70000}]).encode()
        elif self.path.endswith("/empty"):
            code=204
            payload=b""
        self.send_response(code)
        self.send_header("Content-Type","application/json")
        self.send_header("Content-Length",str(len(payload)))
        if code==302:
            self.send_header("Location", "/api/v1/test/redirect-target")
        self.end_headers()
        try:
            self.wfile.write(payload)
        except (ConnectionResetError, BrokenPipeError):
            pass
    do_GET=do_POST=do_PUT=do_PATCH=do_DELETE=handle_request
server=ThreadingHTTPServer(("127.0.0.1",0),Backend)
threading.Thread(target=server.serve_forever,daemon=True).start()
config=runtime/"isolated-test.json"
config.write_text(json.dumps({"wifi":{"ssid":"TEST","password":""},"bambuddy":{"url":f"http://127.0.0.1:{server.server_port}","api_key":"isolated-test-key"},"language":"en"}))
try:
    for args, report in [(["--self-test"],"port-selftest.txt"),(["--transport-test","--config",str(config)],"port-transport-test.txt"),(["--capture"],"original-preview-check.txt")]:
        process=subprocess.run([str(exe),*args],cwd=runtime,timeout=60)
        print((runtime/report).read_text(encoding="utf-8"))
        assert process.returncode==0, (args,process.returncode)
finally:
    server.shutdown()
    server.server_close()
    config.unlink(missing_ok=True)
assert len(events)==11, len(events)
assert all(key=="isolated-test-key" for _,_,key,_ in events), "API key header missing"
assert not any("redirect-target" in path for _,path,_,_ in events), "redirect followed"
for method,path,_,body in events[:5]:
    assert path=="/api/v1/test/echo?language=zh-CN",path
    if method!="GET":
        assert json.loads(body)=={"value":1234.5}
print("PASS HTTP verbs, query, JSON payload, auth header, redirect isolation, errors, size limit and empty 204")
