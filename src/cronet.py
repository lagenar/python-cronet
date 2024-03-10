from dataclasses import dataclass
from functools import cached_property
from typing import Any, Optional, Union
from . import _cronet
import asyncio
import concurrent.futures


class CronetException(Exception):
    pass


@dataclass
class Request:
    url: str
    method: str
    headers: dict[str, str]
    content: bytes


@dataclass
class Response:
    status_code: int
    headers: dict[str, str]
    url: str
    content: Optional[bytes]

    @cached_property
    def text(self):
        return self.content.decode("utf8")


class RequestCallback:
    def __init__(
        self, request: Request, future: Union[asyncio.Future, concurrent.futures.Future]
    ):
        self._response = None
        self._response_content = bytearray()
        self._future = future
        self.request = request

    def _set_result(self, result: Any):
        if isinstance(self._future, concurrent.futures.Future):
            self._future.set_result(result)
        elif isinstance(self._future, asyncio.Future):
            self._future.get_loop().call_soon_threadsafe(
                self._future.set_result, result
            )

    def _set_exception(self, exc: Exception):
        if isinstance(self._future, concurrent.futures.Future):
            self._future.set_exception(exc)
        elif isinstance(self._future, asyncio.Future):
            self._future.get_loop().call_soon_threadsafe(
                self._future.set_exception, exc
            )

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
        self._set_result(self._response_content)

    def on_failed(self, error: str):
        self._set_exception(CronetException(error))

    def on_canceled(self):
        self._set_result(None)


class BaseCronetEngine:
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


class CronetEngine(BaseCronetEngine):
    def request(
        self,
        url: str,
        *,
        method: str = "GET",
        content: Optional[bytes] = None,
        headers: Optional[dict[str, str]] = None,
        timeout: float = 10.0
    ) -> Response:
        req = Request(url=url, method=method, content=content, headers=headers)
        request_future = concurrent.futures.Future()
        callback = RequestCallback(req, request_future)
        cronet_req = self._engine.request(callback)
        try:
            return request_future.result(timeout=timeout)
        except TimeoutError:
            self._engine.cancel(cronet_req)
            raise
        except CronetException:
            raise


class AsyncCronetEngine(BaseCronetEngine):
    async def request(
        self,
        url: str,
        *,
        method: str = "GET",
        content: Optional[bytes] = None,
        headers: Optional[dict[str, str]] = None,
        timeout: float = 10.0
    ) -> Response:
        req = Request(url=url, method=method, content=content, headers=headers)
        request_future = asyncio.Future()
        callback = RequestCallback(req, request_future)
        cronet_req = self._engine.request(callback)
        timeout_task = asyncio.create_task(asyncio.sleep(timeout))
        done, pending = await asyncio.wait(
            [request_future, timeout_task], return_when=asyncio.FIRST_COMPLETED
        )
        for task in pending:
            task.cancel()

        if request_future in done:
            return request_future.result()
        else:
            self._engine.cancel(cronet_req)
            raise TimeoutError()
