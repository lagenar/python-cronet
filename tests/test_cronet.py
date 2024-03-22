import base64
import json

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


def test_send_content(aiohttp_server, cronet_client):
    request_content = b"\xe8\xad\x89\xe6\x98\x8e" * 500
    response = cronet_client.request("GET", f"{BASE_URL}/echo", content=request_content)
    content = json.loads(response.text)["base64_content"]
    assert base64.b64decode(content) == request_content


def test_send_form_data(aiohttp_server, cronet_client):
    data = {"email": "test@example.com", "password": "test"}
    response = cronet_client.request("POST", f"{BASE_URL}/echo", data=data)
    response_data = json.loads(response.text)
    expected_data = {"email": "test@example.com", "password": "test"}
    assert response_data["post_data"] == expected_data
    assert (
        response_data["headers"]["Content-Type"] == "application/x-www-form-urlencoded"
    )


def test_redirect(aiohttp_server, cronet_client):
    response = cronet_client.request(
        "GET", f"{BASE_URL}/redirect", allow_redirects=True
    )
    assert response.url == f"{BASE_URL}/echo"
    assert response.status_code == 200

    response = cronet_client.request(
        "GET", f"{BASE_URL}/redirect", allow_redirects=False
    )
    assert response.status_code == 301
    assert response.url == f"{BASE_URL}/redirect"
    assert response.headers["location"] == "/echo"
