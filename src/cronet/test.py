import transport
import httpx

client = httpx.Client(transport=transport.CronetTransport())
response = client.post("https://httpbin.org/post", json={"test": "true"})
