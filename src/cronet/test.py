import transport
import httpx

client = httpx.Client(transport=transport.CronetTransport())
response = client.get("https://example.com")
