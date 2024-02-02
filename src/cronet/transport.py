import cronet
import httpx


class CronetTransport(httpx.BaseTransport):
    def handle_request(self, request):
        content = cronet.get(str(request.url))
        stream = httpx.ByteStream(content.encode('utf8'))
        return httpx.Response(200, stream=stream)
