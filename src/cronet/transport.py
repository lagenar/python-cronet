import cronet
import httpx


class CronetTransport(httpx.BaseTransport):
    def handle_request(self, request):
        content = cronet.request(str(request.url))
        stream = httpx.ByteStream(content)
        return httpx.Response(200, stream=stream)
