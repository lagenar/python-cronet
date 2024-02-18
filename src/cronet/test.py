import transport
import httpx

headers = {
    "Sec-Fetch-Dest": "document",
    "Sec-Ch-Ua": '"Not A(Brand";v="99", "Google Chrome";v="121", "Chromium";v="121"',
    "Sec-Ch-Ua-Mobile": "?0",
    "Sec-Ch-Ua-Platform": '"Linux"',
    "Upgrade-Insecure-Requests": "1",
    "User-Agent": "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/121.0.0.0 Safari/537.36",
    "Sec-Fetch-Mode": "navigate",
    "Sec-Fetch-User": "?1",
    "Accept": "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.7",
    "Sec-Fetch-Site": "none",
    "Accept-Language": "en-GB,en;q=0.9,es-419;q=0.8,es;q=0.7,pt;q=0.6,it;q=0.5,de;q=0.4",
    "Accept-Encoding": "identity",
}


client = httpx.Client(transport=transport.CronetTransport())
response = client.get("https://wtfismyip.com/json", headers=headers)
print(response.json())
