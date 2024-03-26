# python-cronet


`python-cronet` is a library to use Chromium's network stack from Python.

**The library is currently in alpha stage.**

# What is Cronet

*[Cronet](https://chromium.googlesource.com/chromium/src/+/master/components/cronet/) is the networking stack of Chromium put into a library for use on Android.
It offers an easy-to-use, high performance, standards-compliant, and secure way to perform HTTP requests.*

The Chromium team also provides a native version of the library(not officially supported) which allows you to use
it in desktop/server operating systems like Linux, macOS and Windows.

The main benefits of using cronet as an HTTP client are:
- You get to use the same high quality code that runs on Chromium.
- Support for the latest protocols like QUIC and compression formats.
- Concurrency support by performing asynchronous requests.
- Has the same TLS fingerprint as Chrome, meaning that Cloudflare and other bot detection systems can't block your requests based on it.
- It's much more lightweight on system resources compared to headless Chrome(although it doesn't support executing javascript).

## Installation

**For the time being the only supported platform is linux-x86-64. The plan is to also support windows and macOS.**

`pip install python-cronet`

# Example usage

The library provides an asynchronous API:

```!python
import asyncio
import cronet

async def main():
    with cronet.Cronet() as cr:
        response = await cr.request("GET", "https://example.com")
        print(response.url, response.status_code)

asyncio.run(main())
```

