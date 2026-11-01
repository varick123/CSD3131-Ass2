# Tests

See [TESTING.md](../TESTING.md) for commands, expected results and manual checks. CTest includes `algorithms`, `command_line` and `viewer_http`. Python 3.10+ is needed for the latter two. Core tests use independent exhaustive oracles; CLI tests decode output pixels and replay removals; viewer tests use an isolated loopback server and the real executable.
