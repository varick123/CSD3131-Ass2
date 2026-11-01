# Seam Carving Assignment 2

A self-contained C++17 application implementing dynamic programming, greedy seam selection and genuine graph cut/max-flow. It shrinks width, height or both, recalculating the energy map after every removal. All three algorithms share the same energy, border and removal policies. The source implements Sobel, DP, greedy, residual networks and iterative Dinic directly. Vendored stb headers provide only image decoding/PNG encoding; no content-aware resizing library is used.

## Assignment checklist

See [assignment requirements and rubric](docs/ASSIGNMENT_REQUIREMENTS.md) and [team review tasks](docs/TASKS.md). The implementation was transferred from the existing local project in tested stages on the `varick` branch; `main` retains the starter.

## Build and test on this Windows machine

Open PowerShell in this project folder:

Run these commands directly; they work even when PowerShell blocks `.ps1` scripts and do not require an execution-policy change:

```powershell
$pythonExe = "$env:USERPROFILE\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe"
cmake -S . -B build "-DPython3_EXECUTABLE=$pythonExe"
cmake --build build --config Release --parallel 4
ctest --test-dir build -C Release --output-on-failure
```

The Python path above is the available runtime on this machine; another machine can use its installed Python 3.10+ executable. If scripts are enabled, `.\build.ps1 -Test` is an optional shortcut for the same build/test process.

Prerequisites: CMake 3.20+ and Visual Studio 2022 with C++ desktop tools. Both were available on the development machine. The script builds `build\Release\seamcarve.exe` with Visual Studio, or `build\seamcarve.exe` with a single-configuration generator such as Ninja. Python 3.10+ is used only for integration tests and the viewer; the core executable needs neither Python nor OpenCV. The script detects an available Python runtime and sets the requested build configuration.

Equivalent commands, also useful on other platforms:

```text
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 4
ctest --test-dir build -C Release --output-on-failure
```

On Linux/macOS with a single-configuration generator, the executable is `build/seamcarve`; on the Windows Visual Studio generator it is `build/Release/seamcarve.exe`. Python 3.10+ must be discovered by CMake for the command-line and HTTP viewer tests to be registered. You should see all three tests: `algorithms`, `command_line` and `viewer_http`; one test alone is incomplete verification of the application. Set `-DPython3_EXECUTABLE=<path>` at configure time if needed. The viewer test starts an isolated temporary loopback server on an available port.

## Try the application

```powershell
$app = '.\build\Release\seamcarve.exe'
& $app info --input data/demo/scene_1.png
& $app resize --input data/demo/scene_1.png --output results/my_result.png --width 140 --height 96 --method dp --artifacts results/my_steps
& $app compare --input data/demo/scene_1.png --output-dir results/my_comparison --width 140 --height 96 --repeats 5 --steps
```

Replace `dp` with `greedy` or `graph`. Targets must stay within the original dimensions and at least one pixel. Omit a dimension to leave it unchanged. `--order horizontal-first` changes the two-axis removal order; `vertical-first` is the default. An unchanged target performs no removals.

Outputs are PNG regardless of input format. Common 8-bit PNG/JPEG/BMP/PPM inputs work; grayscale is normalized to RGB and alpha is composited onto white. HDR and 16-bit files are explicitly rejected. Paths containing spaces or Unicode are supported. `info` reads image metadata without decoding pixels. Input/output and JSON-sidecar aliases, including hardlinks, are rejected before writes; use a separate output path. The input must be outside the comparison/artifacts directory.

`compare` writes three resized images, both original-map seam overlays, the original energy map, per-repetition `timings.csv` and `comparison.json`. Each measured repetition starts from the same original and follows one warm-up; the JSON gives median/min/max totals and first-map costs. Stage fields come from the representative first measured run. Timings exclude disk output and visualization; total is the sum of the energy, search and removal stages, excluding setup, output and orchestration overhead. They are not whole-process wall times.

`--artifacts` on `resize`, or `--steps` on `compare`, additionally records every seam, energy map and post-removal image. The JSON uses `axis: "v"` or `"h"`; step width/height are **before** removal. `all_seams.png` maps every deleted pixel to its original position, using red for vertical removal and blue for horizontal removal.

Frame artifact directories must be absent or empty, so reruns cannot mix old frames with a shorter new run. Use a new directory for another recorded run. `compare --steps` checks all method frame directories before writing. If `resize --output` is inside its artifact directory, its filename must be `result.png`; the other artifact filenames are reserved. Ordinary comparisons without `--steps` can refresh their result files using the same comparison directory.

## Interactive viewer

Using the `$pythonExe` variable from the build commands:

```powershell
& $pythonExe .\web\server.py --exe .\build\Release\seamcarve.exe --port 8765
```

If PowerShell scripts are enabled, `.\run_viewer.ps1` is an optional launcher. Keep the server terminal open while using the browser.

The browser opens at `http://127.0.0.1:8765`. Click **Try sample image** or upload `data/demo/scene_1.png`, set both targets, choose an algorithm or compare all three, then run. Use the step slider, next/previous buttons, play, frame-stage selector and algorithm selector to inspect every seam. Download the final PNG while the server is running. Reset restores the original target dimensions; every calculation restarts from the original upload.

The viewer uses only Python's standard library. You can also run `python web/server.py --exe build/Release/seamcarve.exe --port 8765`. Stop it with Ctrl+C. Sessions are temporary and downloads should be saved before stopping. The viewer records all frames, so use modest reductions. Interactive graph comparison is limited to 512 pixels per side and 65,536 input pixels; it never silently scales your upload. Larger inputs can use DP/greedy or the CLI, subject to memory and processing time.

## Learn, reproduce and review

- [Testing guide](TESTING.md): exact commands, expected results, visual inspection and common problems.
- [Class alignment](docs/CLASS_ALIGNMENT.md): all ten lecture PDFs and five tutorials mapped to the implemented techniques, with physical page references.
- [Algorithm analysis](docs/ANALYSIS.md): required answers, proofs, graph boundary correction, time/space costs and improvements.
- [Measured evaluation](results/evaluation/SUMMARY.md): 12 inputs, 10% and 25% reductions, shared-map cost comparisons, raw measurements and scaling tests.
- [Report draft](docs/REPORT_DRAFT.md): technical material to review and complete with team details and links.
- [Presentation/submission guide](docs/PRESENTATION_AND_SUBMISSION.md): demonstration, time limits, quiz preparation and remaining team actions.
- [Input attribution](data/ATTRIBUTION.md) and `third_party/LICENSE-stb.txt`: input and codec provenance.

Reproduce the full evaluation with Python and Pillow installed:

```powershell
python tools/evaluate.py --exe build/Release/seamcarve.exe --repeats 5
```

Pillow is needed for this optional evaluation script's preparation/figures only. Existing evaluation inputs and results are included; the app, viewer and automated tests do not depend on Pillow or network access. `seamcarve demo --output-dir data/demo` regenerates the eight synthetic inputs.

The build/test workflow under `.github/workflows/` is ready to run when this project is placed at the repository root. It tests Windows and Linux and uploads the built executable as a workflow artifact. The workflow is now published on `varick`; check GitHub Actions for its current remote status. Local test success does not establish a remote CI pass or hosted deployment.
