# Tests

Run CTest for `algorithms`, `command_line` and `viewer_http`. Python 3.10+ is required for the latter two. Core tests use independent exhaustive oracles. CLI tests independently decode pixels and replay resizing. Viewer tests launch an isolated loopback server against the real executable and check upload, resizing, frames, downloads and invalid requests.
