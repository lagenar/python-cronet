import asyncio
import concurrent.futures
import json
from dataclasses import dataclass
from functools import cached_property
from typing import Any, Optional, Union
from urllib.parse import ParseResult, parse_qs, urlencode, urlparse

import _cronet


class CronetException(Exception):
    pass


URLParams = Union[str, dict[str, Any], tuple[str, Any]]


@dataclass
class Request:
    url: str
    method: str
    headers: dict[str, str]
    content: bytes

    def add_url_params(self, params: URLParams) -> None:
        if not params:
            return

        request_params = {}
        if isinstance(params, str):
            request_params = parse_qs(params)
        else:
            items = tuple()
            if isinstance(params, dict):
                items = params.items()
            elif isinstance(params, tuple):
                items = params
            else:
                raise TypeError("Received object of invalid type for `params`")
            for k, v in items:
                request_params.setdefault(k, [])
                request_params[k].append(str(v))

        parsed_url = urlparse(self.url)
        if request_params:
            query = parsed_url.query
            url_params = parse_qs(query)
            for k, v in url_params.items():
                request_params.setdefault(k, [])
                request_params[k].extend(v)

            parsed_url = ParseResult(
                scheme=parsed_url.scheme,
                netloc=parsed_url.netloc,
                path=parsed_url.path,
                params=parsed_url.params,
                query=urlencode(request_params, doseq=True),
                fragment=parsed_url.fragment,
            )

        self.url = parsed_url.geturl()


@dataclass
class Response:
    status_code: int
    headers: dict[str, str]
    url: str
    content: Optional[bytes]

    @cached_property
    def text(self):
        return self.content.decode("utf8")

    def json(self) -> Any:
        return json.loads(self.text)


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
        self._set_result(self._response)

    def on_failed(self, error: str):
        self._set_exception(CronetException(error))

    def on_canceled(self):
        self._set_result(None)


class BaseCronet:
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


class Cronet(BaseCronet):
    def request(
        self,
        method: str,
        url: str,
        *,
        params: Optional[URLParams] = None,
        body: Optional[bytes] = None,
        headers: Optional[dict[str, str]] = None,
        timeout: float = 10.0,
    ) -> Response:
        req = Request(method=method, url=url, content=body, headers=headers)
        if params:
            req.add_url_params(params)
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


class AsyncCronet(BaseCronet):
    async def request(
        self,
        method: str,
        url: str,
        *,
        params: Optional[URLParams] = None,
        body: Optional[bytes] = None,
        headers: Optional[dict[str, str]] = None,
        timeout: float = 10.0,
    ) -> Response:
        req = Request(method=method, url=url, content=body, headers=headers)
        if params:
            req.add_url_params(params)
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
