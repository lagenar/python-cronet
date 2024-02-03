import cronet
import httpx


class CronetTransport(httpx.BaseTransport):
    def handle_request(self, request):
        response = cronet.handle_request(str(request.url))
        stream = httpx.ByteStream(response.content)
        return httpx.Response(response.status_code, stream=stream)
