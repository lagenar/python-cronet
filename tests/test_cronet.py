import pytest
from cronet import Cronet
import json


#@pytest.mark.asyncio
def test_get(aiohttp_server):
    cr = Cronet()
    cr.start()
    response = cr.request("GET", "http://127.0.0.1:8080")
    assert response.status_code == 200
    data = json.loads(response.content)
    cr.stop()


