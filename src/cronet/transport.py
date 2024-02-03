import cronet
import httpx


class CronetTransport(httpx.BaseTransport):
    def handle_request(self, request):
        response = cronet.handle_request(str(request.url), request.method, request.content)
        stream = httpx.ByteStream(response.content)
        return httpx.Response(response.status_code, stream=stream, headers=response.headers)
