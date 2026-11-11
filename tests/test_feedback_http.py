"""Loopback-only integration test; never contacts PowerLive or submits real feedback."""
import email.parser
import email.policy
import http.server
import subprocess
import sys
import threading


class Server(http.server.HTTPServer):
    failures = []
    requests = 0


class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass

    def do_POST(self):
        try:
            assert self.path == "/feedback"
            assert self.headers["Cookie"] is None
            assert self.headers["Authorization"] is None
            assert self.headers["X-Feedback-Token"] == "local-test-token"
            content_type = self.headers["Content-Type"]
            assert content_type.startswith("multipart/form-data; boundary=")
            length = int(self.headers["Content-Length"])
            assert 0 < length < 2048
            data = self.rfile.read(length)
            message = email.parser.BytesParser(policy=email.policy.default).parsebytes(
                f"Content-Type: {content_type}\r\n\r\n".encode() + data
            )
            parts = list(message.iter_parts())
            expected = {"message": "中文 @foo; 问题\n第二行 😀"}
            if self.server.requests & 1:
                expected["email"] = "test@example.com"
            if self.server.requests & 2:
                expected["log"] = "UTF-8 诊断日志\nop=message_send_or_recall http=200 api=-101\n"
            assert len(parts) == len(expected)
            actual = {}
            for part in parts:
                name = part.get_param("name", header="Content-Disposition")
                if name == "log":
                    assert part.get_filename() == "diagnostics.log"
                    assert part.get_content_type() == "text/plain"
                    assert part.get_content_charset() == "utf-8"
                else:
                    assert part.get_filename() is None
                actual[name] = part.get_payload(decode=True).decode("utf-8")
            assert actual == expected, (actual, expected)
            self.server.requests += 1
        except Exception as error:
            self.server.failures.append(repr(error))
        self.send_response(429)
        self.send_header("rEtRy-AfTeR", "600")
        self.send_header("Content-Length", "0")
        self.end_headers()


with Server(("127.0.0.1", 0), Handler) as server:
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        subprocess.run(
            [sys.argv[1], "--http", f"http://127.0.0.1:{server.server_port}/feedback"],
            check=True,
            timeout=15,
        )
        assert not server.failures, server.failures
        assert server.requests == 4, server.requests
    finally:
        server.shutdown()
        thread.join()
