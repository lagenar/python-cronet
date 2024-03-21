import base64

from aiohttp import web


async def status_code(request: web.Request):
    return web.Response(text="", status=int(request.match_info["status_code"]))


async def echo(request: web.Request):
    body = await request.read()
    data = {
        "headers": dict(request.headers),
        "url": str(request.rel_url),
        "method": request.method,
        "body": base64.b64encode(body).decode("utf8"),
        #'post_params': dict(request.post())
    }
    return web.json_response(data)


app = web.Application()
app.add_routes(
    [
        web.get(r"/status_code/{status_code:\d+}", status_code),
        web.get("/echo", echo),
    ]
)


if __name__ == "__main__":
    web.run_app(app)
