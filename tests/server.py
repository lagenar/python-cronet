from aiohttp import web


async def status_code(request):
    return web.Response(text="", status=int(request.match_info["status_code"]))


async def headers(request):
    return web.json_response(request.headers)


app = web.Application()
app.add_routes(
    [
        web.get(r"/status_code/{status_code:\d+}", status_code),
        web.get("/headers", headers),
    ]
)


if __name__ == "__main__":
    web.run_app(app)
