#!/usr/bin/env python3
"""Local, dependency-free HTTP interface for the seam-carving executable."""

from __future__ import annotations

import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import re
import subprocess
import tempfile
import threading
from urllib.parse import unquote, urlsplit
import uuid
import webbrowser


UPLOAD_LIMIT = 32 * 1024 * 1024
IMAGE_PIXEL_LIMIT = 16_777_216
IMAGE_SIDE_LIMIT = 8192
GRAPH_PIXEL_LIMIT = 65_536
GRAPH_SIDE_LIMIT = 512
RUN_TIMEOUT = 180
RASTER_EXTENSIONS = {".png", ".jpg", ".jpeg", ".bmp", ".ppm", ".pnm"}
METHODS = {"dp", "greedy", "graph"}
ORDERS = {"vertical-first", "horizontal-first"}


class ApiError(Exception):
    def __init__(self, message: str, status: int = 400):
        super().__init__(message)
        self.status = status


class Session:
    def __init__(self, directory: Path, source: Path, width: int, height: int):
        self.directory = directory
        self.source = source
        self.width = width
        self.height = height
        self.lock = threading.Lock()


class ViewerServer(ThreadingHTTPServer):
    daemon_threads = True

    def __init__(self, address, executable: Path, directory: Path):
        super().__init__(address, ViewerHandler)
        self.executable = executable
        self.directory = directory
        self.sessions: dict[str, Session] = {}
        self.sessions_lock = threading.Lock()

    def invoke(self, arguments: list[str], timeout: int = RUN_TIMEOUT) -> dict:
        try:
            process = subprocess.run(
                [str(self.executable), *arguments],
                cwd=str(self.executable.parent),
                capture_output=True,
                text=True,
                encoding="utf-8",
                errors="replace",
                timeout=timeout,
                creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
                check=False,
            )
        except subprocess.TimeoutExpired as error:
            raise ApiError(
                f"The calculation exceeded {timeout} seconds. Try a smaller input or fewer seams. "
                "Try DP, which avoids max-flow overhead for this objective.", 408
            ) from error
        except OSError as error:
            raise ApiError(f"Could not start the executable: {error}", 500) from error
        if process.returncode != 0:
            message = (process.stderr or process.stdout or "The executable reported an error.").strip()
            raise ApiError(message[-2000:], 422)
        try:
            result = json.loads(process.stdout)
        except json.JSONDecodeError as error:
            raise ApiError("The executable did not return valid JSON. Check the CLI build.", 500) from error
        if not isinstance(result, dict):
            raise ApiError("The executable returned an unexpected JSON result.", 500)
        return result


class ViewerHandler(BaseHTTPRequestHandler):
    server: ViewerServer

    def log_message(self, format, *args):
        print(f"[{self.log_date_time_string()}] {format % args}", flush=True)

    def _trusted_request(self):
        port = self.server.server_address[1]
        allowed_hosts = {f"127.0.0.1:{port}", f"localhost:{port}"}
        if self.headers.get("Host", "") not in allowed_hosts:
            raise ApiError("Use the local viewer address printed by the server.", 403)
        origin = self.headers.get("Origin")
        if origin and origin not in {f"http://{host}" for host in allowed_hosts}:
            raise ApiError("Cross-origin requests are not accepted.", 403)

    def _send(self, status: int, payload: bytes, content_type: str, filename: str | None = None):
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(payload)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("Referrer-Policy", "no-referrer")
        if filename is not None:
            safe_name = re.sub(r"[^A-Za-z0-9_.-]", "_", filename)
            self.send_header("Content-Disposition", f'attachment; filename="{safe_name}"')
        self.end_headers()
        self.wfile.write(payload)

    def _json(self, payload: dict, status: int = 200):
        self._send(status, json.dumps(payload, ensure_ascii=True).encode("utf-8"), "application/json; charset=utf-8")

    def _read_body(self, limit: int) -> bytes:
        try:
            length = int(self.headers.get("Content-Length", "-1"))
        except ValueError as error:
            raise ApiError("Invalid Content-Length header.") from error
        if length < 0:
            raise ApiError("A Content-Length header is required.", 411)
        if length > limit:
            limit_label = f"{limit // (1024 * 1024)} MB" if limit >= 1024 * 1024 else f"{limit // 1024} KB"
            raise ApiError(f"Request is too large. The limit is {limit_label}.", 413)
        self.connection.settimeout(30)
        try:
            payload = self.rfile.read(length)
        except TimeoutError as error:
            raise ApiError("The upload timed out.", 408) from error
        if len(payload) != length:
            raise ApiError("The upload was incomplete.")
        return payload

    def _session(self, token: str) -> Session:
        if not isinstance(token, str) or re.fullmatch(r"[0-9a-f]{32}", token) is None:
            raise ApiError("Upload an image before running a calculation.")
        with self.server.sessions_lock:
            session = self.server.sessions.get(token)
        if session is None:
            raise ApiError("This upload has expired. Please upload the image again.", 404)
        return session

    @staticmethod
    def _file_url(token: str, session: Session, path: Path) -> str:
        relative = path.relative_to(session.directory).as_posix()
        return f"/files/{token}/{relative}"

    def do_GET(self):
        try:
            self._trusted_request()
            request = urlsplit(self.path)
            if request.path in {"/", "/index.html"}:
                page = Path(__file__).with_name("index.html").read_bytes()
                self._send(200, page, "text/html; charset=utf-8")
                return
            if request.path == "/api/health":
                self._json({"status": "ok", "upload_limit": UPLOAD_LIMIT, "graph_pixel_limit": GRAPH_PIXEL_LIMIT})
                return
            if request.path == "/api/sample":
                sample = Path(__file__).resolve().parent.parent / "data" / "demo" / "scene_1.png"
                if not sample.is_file():
                    raise ApiError("The bundled sample image is unavailable. Generate the demo images first.", 404)
                self._send(200, sample.read_bytes(), "image/png")
                return
            if request.path.startswith("/files/"):
                parts = unquote(request.path).split("/", 3)
                if len(parts) != 4:
                    raise ApiError("File not found.", 404)
                session = self._session(parts[2])
                path = (session.directory / parts[3]).resolve()
                try:
                    path.relative_to(session.directory.resolve())
                except ValueError as error:
                    raise ApiError("File not found.", 404) from error
                if path.suffix.lower() not in {".png", ".json"} or not path.is_file():
                    raise ApiError("File not found.", 404)
                content_type = "image/png" if path.suffix.lower() == ".png" else "application/json; charset=utf-8"
                download_name = path.name if request.query == "download=1" else None
                self._send(200, path.read_bytes(), content_type, download_name)
                return
            raise ApiError("Page not found.", 404)
        except ApiError as error:
            self._json({"error": str(error)}, error.status)
        except (BrokenPipeError, ConnectionResetError):
            pass
        except OSError as error:
            self._json({"error": f"Could not read the requested file: {error}"}, 500)

    def do_POST(self):
        try:
            path = urlsplit(self.path).path
            # Consume bounded bodies before returning validation errors. Closing
            # a connection with unread incoming bytes can abort Windows clients
            # before they receive the JSON error response.
            payload = self._read_body(UPLOAD_LIMIT if path == "/api/upload" else 64 * 1024)
            self._trusted_request()
            if path == "/api/upload":
                self._upload(payload)
            elif path == "/api/run":
                self._run(payload)
            else:
                raise ApiError("Endpoint not found.", 404)
        except ApiError as error:
            self._json({"error": str(error)}, error.status)
        except (BrokenPipeError, ConnectionResetError):
            pass
        except (OSError, ValueError, KeyError, TypeError) as error:
            self._json({"error": f"Could not process the request: {error}"}, 500)

    def _upload(self, payload: bytes):
        filename = unquote(self.headers.get("X-Filename", "image.png"))
        extension = Path(filename).suffix.lower()
        if extension not in RASTER_EXTENSIONS:
            raise ApiError("Choose a raster image such as PNG, JPEG, BMP or PPM.")
        if not payload:
            raise ApiError("The uploaded file is empty.")
        token = uuid.uuid4().hex
        directory = self.server.directory / token
        directory.mkdir()
        source = directory / ("source" + extension)
        source.write_bytes(payload)
        info = self.server.invoke(["info", "--input", str(source)], timeout=30)
        width, height = info.get("width"), info.get("height")
        if type(width) is not int or type(height) is not int or width < 1 or height < 1:
            raise ApiError("The executable reported invalid image dimensions.", 500)
        if width > IMAGE_SIDE_LIMIT or height > IMAGE_SIDE_LIMIT or width * height > IMAGE_PIXEL_LIMIT:
            raise ApiError("This interactive viewer accepts images up to 8192 pixels per side and 16,777,216 pixels total.")
        preview = directory / "preview.png"
        self.server.invoke([
            "resize", "--input", str(source), "--output", str(preview),
            "--width", str(width), "--height", str(height), "--method", "dp",
            "--order", "vertical-first",
        ], timeout=30)
        session = Session(directory, source, width, height)
        with self.server.sessions_lock:
            self.server.sessions[token] = session
        self._json({
            "token": token, "filename": Path(filename).name,
            "width": width, "height": height,
            "preview": self._file_url(token, session, preview),
        })

    def _run(self, payload: bytes):
        if self.headers.get("Content-Type", "").split(";", 1)[0].strip() != "application/json":
            raise ApiError("Send the run settings as application/json.", 415)
        try:
            settings = json.loads(payload)
        except (json.JSONDecodeError, UnicodeDecodeError) as error:
            raise ApiError("Run settings must be valid JSON.") from error
        if not isinstance(settings, dict):
            raise ApiError("Run settings must be a JSON object.")
        token = settings.get("token")
        session = self._session(token)
        width, height = settings.get("width"), settings.get("height")
        if type(width) is not int or type(height) is not int:
            raise ApiError("Target width and height must be whole numbers.")
        if not (1 <= width <= session.width and 1 <= height <= session.height):
            raise ApiError("Targets must be positive and cannot exceed the original dimensions. This viewer removes seams.")
        method, order = settings.get("method", "all"), settings.get("order", "vertical-first")
        if (not isinstance(method, str) or not isinstance(order, str) or
                method not in METHODS | {"all"} or order not in ORDERS):
            raise ApiError("Choose a supported algorithm and removal order.")
        if method in {"all", "graph"} and (
            session.width > GRAPH_SIDE_LIMIT or session.height > GRAPH_SIDE_LIMIT or
            session.width * session.height > GRAPH_PIXEL_LIMIT
        ):
            raise ApiError(
                "Graph cut in this interactive viewer is limited to 512 pixels per side and "
                "65,536 input pixels total. Upload a smaller image, or choose DP or greedy. "
                "The viewer does not automatically scale your input."
            )
        if not session.lock.acquire(blocking=False):
            raise ApiError("A calculation is already running for this upload. Please wait for it to finish.", 409)
        try:
            run_id = uuid.uuid4().hex
            directory = session.directory / "runs" / run_id
            directory.mkdir(parents=True)
            common = ["--input", str(session.source), "--width", str(width), "--height", str(height), "--order", order]
            if method == "all":
                summary = self.server.invoke([
                    "compare", *common, "--output-dir", str(directory), "--repeats", "1", "--steps",
                ])
                entries = summary.get("methods", [])
            else:
                artifacts = directory / method
                artifacts.mkdir()
                summary = self.server.invoke([
                    "resize", *common, "--output", str(artifacts / "result.png"),
                    "--method", method, "--artifacts", str(artifacts),
                ])
                entries = [summary]
            if not isinstance(entries, list):
                raise ApiError("The executable returned an invalid method list.", 500)
            results = []
            for entry in entries:
                if not isinstance(entry, dict) or entry.get("method") not in METHODS:
                    raise ApiError("The executable returned an invalid algorithm result.", 500)
                name = entry["method"]
                artifacts = directory / name
                output = directory / (name + ".png") if method == "all" else artifacts / "result.png"
                steps = entry.get("steps", [])
                if not isinstance(steps, list):
                    raise ApiError("The executable returned an invalid step list.", 500)
                frames = []
                for index, step in enumerate(steps, 1):
                    frame = dict(step)
                    for stage in ("seam", "energy", "after"):
                        frame[stage + "_url"] = self._file_url(token, session, artifacts / f"{index:04d}_{stage}.png")
                    frames.append(frame)
                results.append({
                    **entry, "frames": frames,
                    "output_url": self._file_url(token, session, output),
                    "all_seams_url": self._file_url(token, session, artifacts / "all_seams.png"),
                    "original_url": self._file_url(token, session, artifacts / "original.png"),
                })
            self._json({
                "run": run_id, "summary": summary, "methods": results,
                "original": self._file_url(token, session, session.directory / "preview.png"),
                "input_width": session.width, "input_height": session.height,
                "target_width": width, "target_height": height, "order": order,
            })
        finally:
            session.lock.release()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", required=True, type=Path, help="Path to the built seam-carving executable")
    parser.add_argument("--port", default=8765, type=int, help="Loopback HTTP port (default: 8765)")
    parser.add_argument("--no-open", action="store_true", help="Do not open a browser automatically")
    args = parser.parse_args()
    executable = args.exe.expanduser().resolve()
    if not executable.is_file():
        parser.error(f"Executable not found: {executable}")
    if not 1 <= args.port <= 65535:
        parser.error("The port must be between 1 and 65535.")
    with tempfile.TemporaryDirectory(prefix="seam-viewer-") as temporary:
        try:
            server = ViewerServer(("127.0.0.1", args.port), executable, Path(temporary))
        except OSError as error:
            parser.error(f"Could not start the local server: {error}")
        url = f"http://127.0.0.1:{args.port}"
        print(f"Seam viewer: {url}\nExecutable: {executable}\nPress Ctrl+C to stop. Sessions are temporary.", flush=True)
        if not args.no_open:
            threading.Timer(0.5, lambda: webbrowser.open(url)).start()
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            print("\nStopping the viewer.", flush=True)
        finally:
            server.server_close()


if __name__ == "__main__":
    main()
