from dataclasses import dataclass
from functools import cached_property
from typing import Optional
from . import _cronet
import threading



class CronetException(Exception):
    pass


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
        self._exc = None

    def on_redirect_received(self, location: str):
        pass

    def on_response_started(self, url: str, status_code: int, headers: dict[str, str]):
        self._response = Response(
            url=url, status_code=status_code, headers=headers, content=None
        )

    def on_read_completed(self, data: bytes):
        self._response_content.extend(data)

    def on_succeeded(self):
        self._response.content = bytes(self._response_content)
        self._done.set()

    def on_failed(self, error: str):
        self._exc = CronetException(error)
        self._done.set()

    def on_canceled(self):
        self._done.set()

    def wait_until_done(self, timeout: Optional[float] = None):
        if not self._done.wait(timeout=timeout):
            raise TimeoutError()
        if self._exc:
            raise self._exc

    @property
    def response(self):
        return self._response


@dataclass
class Response:
    status_code: int
    headers: dict[str, str]
    url: str
    content: Optional[bytes]

    @cached_property
    def text(self):
        return self.content.decode("utf8")


class Cronet:
    def __init__(self):
        self._engine = None

    def __enter__(self):
        self.start()
        return self

    def __exit__(self, *args):
        self.stop()

    def start(self):
        if not self._engine:
            self._engine = _cronet.CronetEngine()

    def stop(self):
        if self._engine:
            del self._engine

    def request(
        self,
        url: str,
        *,
        method: str = "GET",
        content: Optional[bytes] = None,
        headers: Optional[dict[str, str]] = None,
        timeout: Optional[float] = None
    ):
        req = Request(url=url, method=method, content=content, headers=headers)
        cronet_req = self._engine.request(req)
        try:
            req.wait_until_done(timeout=timeout)
        except TimeoutError:
            self._engine.cancel(cronet_req)
            req.wait_until_done()
            raise
        except CronetException:
            raise

        return req.response


if __name__ == "__main__":
    with Cronet() as cr:
        response = cr.request("https://httpbin.org/get", timeout=5.0)
        print(response)
