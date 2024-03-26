import asyncio
import concurrent.futures
import json
from dataclasses import dataclass
from functools import cached_property
from typing import Any, Optional, Union
from urllib.parse import ParseResult, parse_qs, urlencode, urlparse

from multidict import CIMultiDict

from . import _cronet


class CronetException(Exception):
    pass


URLParams = Union[str, dict[str, Any], tuple[str, Any]]


@dataclass
class Request:
    url: str
    method: str
    headers: dict[str, str]
    content: bytes
    allow_redirects: bool = True

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

    def set_form_data(self, data: dict[str, str]):
        self.content = urlencode(data).encode("utf8")
        self.headers["Content-Type"] = "application/x-www-form-urlencoded"

    def set_json_data(self, data: Any):
        self.content = json.dumps(data).encode("utf8")
        self.headers["Content-Type"] = "application/json"


@dataclass
class Response:
    status_code: int
    headers: CIMultiDict
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

    def on_redirect_received(
        self,
        url: str,
        new_location: str,
        status_code: int,
        headers: list[tuple[str, str]],
    ):
        self._response = Response(
            url=url, status_code=status_code, headers=CIMultiDict(headers), content=b""
        )
        if not self.request.allow_redirects:
            self._set_result(self._response)

    def on_response_started(
        self, url: str, status_code: int, headers: list[tuple[str, str]]
    ):
        self._response = Response(
            url=url, status_code=status_code, headers=CIMultiDict(headers), content=None
        )

    def on_read_completed(self, data: bytes):
        self._response_content.extend(data)

    def on_succeeded(self):
        self._response.content = bytes(self._response_content)
        self._set_result(self._response)

    def on_failed(self, error: str):
        self._set_exception(CronetException(error))

    def on_canceled(self):
        self._set_result(self._response)


class Cronet:
    def __init__(self, proxy_settings: Optional[str] = None):
        self._engine = None
        self.proxy_settings = proxy_settings

    def __enter__(self):
        self.start()
        return self

    def __exit__(self, *args):
        self.stop()

    def start(self):
        if not self._engine:
            self._engine = _cronet.CronetEngine(self.proxy_settings)

    def stop(self):
        if self._engine:
            del self._engine

    async def request(
        self,
        method: str,
        url: str,
        *,
        params: Optional[URLParams] = None,
        data: Optional[dict[str, str]] = None,
        content: Optional[bytes] = None,
        json: Optional[Any] = None,
        headers: Optional[dict[str, str]] = None,
        allow_redirects: bool = True,
        timeout: float = 10.0,
    ) -> Response:
        if len([c_arg for c_arg in [data, content, json] if c_arg]) > 1:
            raise ValueError("Only one of `data`, `content` and `json` can be provided")

        self.start()
        req = Request(
            method=method,
            url=url,
            content=content,
            headers=headers or {},
            allow_redirects=allow_redirects,
        )
        if params:
            req.add_url_params(params)
        if data:
            req.set_form_data(data)
        elif json:
            req.set_json_data(json)
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

    async def get(self, url: str, **kwargs) -> Response:
        return await self.request("GET", url, **kwargs)

    async def post(self, url: str, **kwargs) -> Response:
        return await self.request("POST", url, **kwargs)

    async def put(self, url: str, **kwargs) -> Response:
        return await self.request("PUT", url, **kwargs)

    async def patch(self, url: str, **kwargs) -> Response:
        return await self.request("PATCH", url, **kwargs)

    async def delete(self, url: str, **kwargs) -> Response:
        return await self.request("DELETE", url, **kwargs)
