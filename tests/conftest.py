import os
import sys
import pytest
import subprocess
import time
import http.client



def wait_server_ready():
    attempts = 5
    while True:
        try:
            conn = http.client.HTTPConnection("127.0.0.1", 8080)
            conn.request("GET", "/")
        except Exception:
            attempts -=1
            if not attempts:
                raise
            else:
                time.sleep(0.5)
                continue
        else:
            break


@pytest.fixture(scope="session")
def aiohttp_server():
    process = subprocess.Popen((sys.executable, 'tests/server.py'))
    wait_server_ready()
    yield
    process.terminate()
    process.wait()
