#!/usr/bin/env python3
"""Static server for the shared-memory WebAssembly build."""

import argparse
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path


class SharedMemoryHandler(SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        self.send_header("Cross-Origin-Resource-Policy", "same-origin")
        super().end_headers()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--directory", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1",
                        help="Interface to listen on (default: loopback only)")
    parser.add_argument("--port", type=int, default=8080)
    args = parser.parse_args()
    handler = lambda *request: SharedMemoryHandler(*request, directory=args.directory)
    server = ThreadingHTTPServer((args.host, args.port), handler)
    display_host = "localhost" if args.host == "127.0.0.1" else args.host
    print(f"Serving {args.directory} at http://{display_host}:{args.port}")
    server.serve_forever()


if __name__ == "__main__":
    main()
