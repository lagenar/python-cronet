from dataclasses import dataclass
from typing import Optional
import _cronet
import threading


@dataclass
class Request:
    url: str
    method: str
    content: bytes
    headers: dict[str, str]

    def __post_init__(self):
        self._response = None
        self._response_content = bytearray()
        self._done = threading.Event()

    def on_redirect_received(self, location: str):
        print("redirected to", location)

    def on_response_started(self, status_code: int, headers: dict[str, str]):
        self._response = Response(status_code=status_code, headers=headers, 
                                  content=None)

    def on_read_completed(self, data: bytes):
        self._response_content.extend(data)

    def on_succeeded(self):
        print("Success")
        self._response.content = bytes(self._response_content)
        self._done.set()

    def on_failed(self, error: str):
        print('Request failed', error)
        self._done.set()

    def on_canceled(self):
        print('request canceled')
        self._done.set()
    
    def wait_until_done(self):
        self._done.wait()

    @property
    def response(self):
        return self._response



@dataclass
class Response:
    status_code: int
    headers: dict[str, str]
    #url: str
    content: Optional[bytes]


class Cronet:
    def __init__(self):
        self._engine = None

    def __enter__(self):
        self.start()
        return self

    def __exit__(self, *args):
        self.stop()

    def start(self):
        self._engine = _cronet.CronetEngine()

    def stop(self):
        if self._engine:
            del self._engine

    def request(self, url, *, method="GET", content=None, headers=None):
        req = Request(url=url, method=method, content=content, headers=headers)
        self._engine.request(req)
        req.wait_until_done()
        return req.response


if __name__ == '__main__':
    with Cronet() as cr:
        response = cr.request("https://example.com")
        print(response)

