import cronet
import httpx


cronet_client = cronet.Cronet()


'''
UrlRequestCallback::OnRedirectReceived,
          UrlRequestCallback::OnResponseStarted,
          UrlRequestCallback::OnReadCompleted,
          UrlRequestCallback::OnSucceeded,
          UrlRequestCallback::OnFailed,
          UrlRequestCallback::OnCanceled
'''


class Callbacks():
    def on_response_started(self, status_code: int, headers: dict[str, str]):
        print("Got response from cronet with status code", status_code, 'headers', headers)

    def on_redirect_received(self):
        pass

    def on_read_completed(self, data: bytes):
        #print(data)
        pass

    def on_succeeded(self):
        print("done.")

    def on_failed(self, error: str):
        print('Failed with', error)

    def on_cancelled(self):
        pass



class CronetTransport(httpx.BaseTransport):
    def handle_request(self, request):
        response = cronet_client.handle_request(str(request.url), request.method, request.content, dict(request.headers), Callbacks())
        stream = httpx.ByteStream(response.content)
        return httpx.Response(response.status_code, stream=stream, headers=response.headers)
