from dataclasses import dataclass
import cronet

@dataclass
class Request:
    url: str
    method: str

    def on_redirect_received(self, location: str):
        print("redirected to", location)

    def on_response_started(self, status_code: int, headers: dict[str, str]):
        print("response started", status_code, headers)

    def on_read_completed(self, data: bytes):
        print("read completed", data)

    def on_succeeded(self):
        print("Success")

    def on_failed(self, error: str):
        print('Request failed', error)

    def on_canceled(self):
        print('request canceled')


req = Request(url='https://google.com', method='GET')
engine = cronet.Cronet()
engine.request(req)

import time
time.sleep(10)
