import base64
import json

import cronet
import pytest

BASE_URL = "http://127.0.0.1:8080"


@pytest.mark.asyncio
async def test_status_code(aiohttp_server, cronet_client):
    response = await cronet_client.request("GET", f"{BASE_URL}/status_code/200")
    assert response.status_code == 200
    response = await cronet_client.request("GET", f"{BASE_URL}/status_code/404")
    assert response.status_code == 404


@pytest.mark.asyncio
async def test_send_headers(aiohttp_server, cronet_client):
    headers = {
        "a": "1",
        "b": "2",
        "User-Agent": "cronet",
        "Accept-Encoding": "*",
        "Host": "127.0.0.1:8080",
        "Connection": "keep-alive",
    }
    response = await cronet_client.request("GET", f"{BASE_URL}/echo", headers=headers)
    assert response.json()["headers"] == headers


@pytest.mark.asyncio
async def test_case_insentive_headers(aiohttp_server, cronet_client):
    response = await cronet_client.request("GET", f"{BASE_URL}/echo")
    assert response.headers["content-type"] == "application/json; charset=utf-8"
    assert response.headers["Content-Type"] == "application/json; charset=utf-8"


@pytest.mark.asyncio
async def test_send_params(aiohttp_server, cronet_client):
    response = await cronet_client.request(
        "GET", f"{BASE_URL}/echo", params={"test1": "1", "test2": "2"}
    )
    assert response.url == f"{BASE_URL}/echo?test1=1&test2=2"
    assert response.json()["url"] == "/echo?test1=1&test2=2"


@pytest.mark.asyncio
async def test_send_content(aiohttp_server, cronet_client):
    request_content = b"\xe8\xad\x89\xe6\x98\x8e" * 3550
    response = await cronet_client.request(
        "GET", f"{BASE_URL}/echo", content=request_content
    )
    content = json.loads(response.text)["base64_content"]
    assert base64.b64decode(content) == request_content


@pytest.mark.asyncio
async def test_send_form_data(aiohttp_server, cronet_client):
    data = {"email": "test@example.com", "password": "test"}
    response = await cronet_client.request("POST", f"{BASE_URL}/echo", data=data)
    response_data = json.loads(response.text)
    assert response_data["post_data"] == data
    assert (
        response_data["headers"]["Content-Type"] == "application/x-www-form-urlencoded"
    )


@pytest.mark.asyncio
async def test_send_json_data(aiohttp_server, cronet_client):
    data = {"form": {"email": "test@example.com", "password": "test"}}
    response = await cronet_client.request("POST", f"{BASE_URL}/echo", json=data)
    response_data = json.loads(response.text)
    json_data = base64.b64decode(response_data["base64_content"])
    assert json.loads(json_data) == data
    assert response_data["headers"]["Content-Type"] == "application/json"


@pytest.mark.asyncio
async def test_redirect(aiohttp_server, cronet_client):
    response = await cronet_client.request(
        "GET", f"{BASE_URL}/redirect", allow_redirects=True
    )
    assert response.url == f"{BASE_URL}/echo"
    assert response.status_code == 200

    response = await cronet_client.request(
        "GET", f"{BASE_URL}/redirect", allow_redirects=False
    )
    assert response.status_code == 301
    assert response.url == f"{BASE_URL}/redirect"
    assert response.headers["location"] == "/echo"
