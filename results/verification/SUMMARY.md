# Verification record - 20 October 2026

- C++17 application built with Visual Studio 2022 / MSVC 19.44 in Release and Debug.
- CTest: **3/3 passed in both configurations** after the audit fixes: core algorithms, command-line integration and the HTTP viewer.
- Core suite: **43,477 assertions**, including 1,689 seam maps checked by exhaustive legal-path enumeration, 205 flow graphs checked by exhaustive cuts, and 207 Sobel images checked by independent convolution.
- Integration suite: **8,624 assertions**, 88 command invocations and 38 independently replayed resize sequences on Windows. Checks include Unicode paths, both axis orders, exact current energies, pixel survival order, frame dimensions, original-coordinate overlays, grayscale/alpha conversion, unsupported depths, invalid inputs and output preservation. All three hardlink fixture cases ran on the verified machine; other filesystems explicitly report skips if unsupported.
- HTTP viewer suite: **six tests and 130 requests** using the real executable; covers every method/frame stage, metadata limits before decoding, sample/upload/download, unchanged targets, malformed settings, concurrency and file boundaries.
- Added a focused bent-horizontal-seam regression. A 3x4 grayscale input has DP/graph first horizontal seam `[0,1,1,2]`; horizontal-first resizing to 2x1 exercises coordinate mapping after this seam. All three methods pass the exact original-coordinate overlay check.
- Viewer: sample/upload, three-method resize to 152x108, vertical and horizontal playback, energy/seam/after selection, method switching, play/pause and final-size captions checked. `viewer-tested.png` shows the final horizontal frame. Session output files are served locally while the viewer runs.
- Evaluation: 12 prepared inputs, 24 resize cases, 360 timed full-resize repetitions and 24 scaling method/orientation summaries. DP/graph shared-map costs agree in both orientations. Input hashes, raw measurements, configuration and figures are included.
- Fresh output reproduction: **all 96 evaluation and scaling result images matched the recorded outputs pixel-for-pixel** with the audited executable. All raw timing statistics and input hashes were independently checked for consistency.
- Independent audit stress harness: **81,032 checks**, including 2,765 flow graphs against separate Edmonds-Karp, 1,920 maps against exhaustive seam enumeration, a 300,000-vertex path, a 50,000-column seam graph and near-limit capacities. Harness/evidence remain in the workspace's `tmp/ass2-audit` folder.
- Build-script portability: an isolated Ninja copy, initially configured Debug, was built with the unchanged `build.ps1 -Test`. Its cache changed to Release with `/O2` and all three tests passed. The ordinary Visual Studio Release and Debug builds also pass.
- Audit corrections: metadata-only size inspection; malformed viewer settings return client errors; destination aliases are rejected before writes; nonempty frame directories are rejected; generator-specific build flags and executable discovery are handled; future evaluation metadata records the detected compiler/configuration rather than a hardcoded label.
- GitHub repository was created, but its contents were left unchanged and no push was performed. The workflow has not been executed remotely. Source upload, recording/uploading the presentation, final team review and peer evaluation remain team actions.

Raw local CTest output is retained alongside this record as `release-tests.txt` and `debug-tests.txt`; packaging omits these machine-specific logs while retaining this summary and the screenshot.

## Windows upload-response regression

A user-run viewer test exposed WinError 10053 when a rejected POST returned before consuming its request body. The old handler reproduced both the connection abort and a failing delayed-body regression. Bounded POST bodies are now consumed once before route/type/filename validation sends a response. The regression sends upload headers and body separately and verifies a readable HTTP 400 response for the unsupported file. No client retries or skipped failures were added.

After the response-order correction, all three suites passed in Release and Debug, and viewer_http passed ten consecutive runs (six cases, 130 requests per run).

## Final local closeout - 21 October 2026 (Singapore)

The final Release check passed all three suites. Primary build/test/viewer instructions now use direct CMake/Python commands, so the known PowerShell script restriction requires no policy change. Source and package consistency were checked; repository publication and remaining student submission tasks are still deferred.


## Branch publication - 21 October 2026

The notes above describe earlier local checks; their statements that publication was deferred are historical. The existing implementation was subsequently transferred to the `varick` branch in tested commits. The branch build passed all three Release CTest suites after transfer. Windows sandbox restrictions initially blocked loopback HTTP access; rerunning outside that sandbox passed the viewer suite. Evaluation data retains its actual original measurement dates and environment; it was not newly measured for publication. The local audit harness mentioned above is workspace-only, and raw historical logs are not part of this repository. Remote CI status must be checked on GitHub separately.
