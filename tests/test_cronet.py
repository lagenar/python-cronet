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
    response = cronet_client.request("GET", f"{BASE_URL}/headers", headers=headers)
    assert json.loads(response.text) == headers


def test_send_params(aiohttp_server):
    pass


def test_send_body(aiohttp_server):
    pass
