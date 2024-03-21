import base64
import json

import pytest
from cronet import Cronet

BASE_URL = "http://127.0.0.1:8080"


# @pytest.mark.asyncio
def test_status_code(aiohttp_server, cronet_client):
    response = cronet_client.request("GET", f"{BASE_URL}/status_code/200")
    assert response.status_code == 200
    response = cronet_client.request("GET", f"{BASE_URL}/status_code/404")
    assert response.status_code == 404


def test_send_headers(aiohttp_server, cronet_client):
    headers = {
        "a": "1",
        "b": "2",
        "User-Agent": "cronet",
        "Accept-Encoding": "*",
        "Host": "127.0.0.1:8080",
        "Connection": "keep-alive",
    }
    response = cronet_client.request("GET", f"{BASE_URL}/echo", headers=headers)
    assert response.json()["headers"] == headers


def test_send_params(aiohttp_server, cronet_client):
    response = cronet_client.request(
        "GET", f"{BASE_URL}/echo", params={"test1": "1", "test2": "2"}
    )
    assert response.url == f"{BASE_URL}/echo?test1=1&test2=2"
    assert response.json()["url"] == "/echo?test1=1&test2=2"


def test_send_body(aiohttp_server, cronet_client):
    request_body = b"\xe8\xad\x89\xe6\x98\x8e" * 500
    response = cronet_client.request("GET", f"{BASE_URL}/echo", body=request_body)
    body = json.loads(response.text)["body"]
    assert base64.b64decode(body) == request_body
