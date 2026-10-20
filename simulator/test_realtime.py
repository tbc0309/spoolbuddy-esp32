"""Production WinHTTP WebSocket transport against disposable ASGI socket fixtures."""
import asyncio
import json
import socket
import subprocess
import threading
import time
from pathlib import Path

import uvicorn
from fastapi import FastAPI, WebSocket
from fastapi.responses import JSONResponse

ROOT = Path(__file__).resolve().parent
RUNTIME = ROOT / 'runtime'
RUNTIME.mkdir(exist_ok=True)
EXE = ROOT / 'build/spoolbuddy_sim.exe'


def run_case(mode):
    app = FastAPI()
    events = []
    token = 'isolated+a/b=token'

    @app.post('/api/v1/auth/ws-token')
    async def auth():
        events.append('auth')
        if mode == 'http_denied':
            return JSONResponse({'detail': 'denied'}, status_code=403)
        return {'token': token}

    @app.websocket('/api/v1/ws')
    async def ws(peer: WebSocket):
        assert peer.query_params.get('token') == token
        await peer.accept()
        events.append('socket')
        if mode == 'socket_denied':
            await peer.close(code=4401)
            return
        await peer.send_json({'type': 'printer_status', 'printer_id': 3,
                              'data': {'connected': True, 'ams': [], 'payload': 'x' * 12000}})
        await asyncio.sleep(.15)
        await peer.close()

    sock = socket.socket()
    sock.bind(('127.0.0.1', 0))
    port = sock.getsockname()[1]
    server = uvicorn.Server(uvicorn.Config(app, lifespan='off', log_level='critical', access_log=False))
    thread = threading.Thread(target=server.run, kwargs={'sockets': [sock]}, daemon=True)
    thread.start()
    config = RUNTIME / 'realtime-test.json'
    config.write_text(json.dumps({'wifi': {'ssid': 'TEST', 'password': ''},
        'bambuddy': {'url': f'http://127.0.0.1:{port}', 'api_key': ''}, 'language': 'en'}), encoding='utf-8')
    try:
        deadline = time.monotonic() + 10
        while not server.started and time.monotonic() < deadline:
            time.sleep(.02)
        assert server.started
        flag = '--realtime-test' if mode == 'normal' else '--realtime-denied-test'
        process = subprocess.run([str(EXE), flag, '--config', str(config)], cwd=RUNTIME, timeout=20)
        report = (RUNTIME / 'port-realtime-test.txt').read_text(encoding='utf-8')
        assert process.returncode == 0, report
        if mode == 'normal':
            assert events.count('socket') >= 2, events
        elif mode == 'socket_denied':
            assert events == ['auth', 'socket'], events
        else:
            assert events == ['auth'], events
        print(mode, report.strip(), events)
        return {"mode":mode,"status":"PASS","events":events}
    finally:
        server.should_exit = True
        thread.join(timeout=5)
        sock.close()
        config.unlink(missing_ok=True)


if __name__ == '__main__':
    suite=RUNTIME/'port-realtime-suite.json'
    suite.unlink(missing_ok=True)
    cases=[run_case(mode) for mode in ('normal','socket_denied','http_denied')]
    suite.write_text(json.dumps({"status":"PASS","cases":cases,"hardware_tested":False},indent=2),encoding='utf-8')
