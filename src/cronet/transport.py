import cronet
import httpx
import asyncio


cronet_client = cronet.Cronet()


class CronetRequest():
    def __init__(self, url: str, method: str, content: bytes, headers: dict[str, str]):
        self.url = url
        self.method = method
        self.content = content
        self.headers = headers
        self._response = None
        
        self._response_status_code = None
        self._response_headers = None
        self._response_content = None
    
    def start(self) -> asyncio.Future:
        self._response = asyncio.Future()
        cronet_client.handle_request(str(self.url), self.method, self.content, dict(self.headers), self)
        return self._response

    def on_response_started(self, status_code: int, headers: dict[str, str]):
        print("Got response from cronet with status code", status_code, 'headers', headers)
        self._response_status_code = status_code
        self._response_headers = headers

    def on_redirect_received(self):
        pass

    def on_read_completed(self, content: bytes):
        if self._response_content is None:
            self._response_content = content
        else:
            self._response_content += content

    def on_succeeded(self):
        loop = self._response.get_loop()
        resp = {"status_code": self._response_status_code, 
                "headers": self._response_headers, "content": self._response_content}
        
        loop.call_soon_threadsafe(self._response.set_result, resp)

    def on_failed(self, error: str):
        print('Failed with', error)

    def on_cancelled(self):
        pass



class CronetTransport(httpx.BaseTransport):
    def handle_request(self, request):
        response = cronet_client.handle_request(str(request.url), request.method, request.content, dict(request.headers), Callbacks())
        stream = httpx.ByteStream(response.content)
        return httpx.Response(response.status_code, stream=stream, headers=response.headers)



class AsyncCronetTransport(httpx.AsyncBaseTransport):
    async def handle_async_request(self, request: httpx.Request) -> httpx.Response:
        cronet_request = CronetRequest(request.url, request.method, request.content, request.headers)
        response_future = cronet_request.start()
        response = await response_future
        stream = httpx.ByteStream(response['content'].encode('utf8'))
        return httpx.Response(response['status_code'], stream=stream, headers=response['headers'])
