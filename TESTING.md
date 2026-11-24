# How to test and check the assignment

Run these commands from the project folder in PowerShell. Keep the original images unchanged. No internet connection is required for the app, viewer or tests.

## 1. Rebuild and run all automated suites

```powershell
$pythonExe = "$env:USERPROFILE\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe"
cmake -S . -B build "-DPython3_EXECUTABLE=$pythonExe"
cmake --build build --config Release --parallel 4
ctest --test-dir build -C Release --output-on-failure
```

These direct commands work with this machine's restricted PowerShell script policy. Another machine can substitute its installed Python 3.10+ executable. Where scripts are enabled, `.\build.ps1 -Test` is an optional shortcut.

Expected CTest result: **3/3 tests passed** (`algorithms`, `command_line` and `viewer_http`). The current Release and Debug runs passed:

- 43,477 C++ assertions: 1,689 seam maps versus independent exhaustive seam enumeration; 205 flow graphs versus exhaustive cuts; 207 Sobel images versus independent convolution.
- 8,624 integration assertions: 88 command invocations and 38 independently replayed resize sequences on the checked Windows machine. The test decodes PNG data independently, recomputes current energies and checks surviving pixel order, frame sizes, original-coordinate overlays, invalid inputs, Unicode paths, bent horizontal seams, grayscale/alpha conversions, unsupported bit depths and output preservation. Hardlink fixtures executed here; on filesystems that cannot create them, the test explicitly reports those three cases as skipped.
- Six HTTP viewer tests checking 130 requests against the real executable: sample/upload, individual/all algorithms, every frame stage, downloads, unchanged targets, malformed settings, size limits before decode, concurrent-run rejection and file boundaries.

For the detailed output:

```powershell
.\build\Release\seam_tests.exe
ctest --test-dir build -C Release -V
```

CTest saves its latest output in `build/Testing/Temporary/LastTest.log`. To check Debug, run `cmake --build build --config Debug --parallel 4` followed by `ctest --test-dir build -C Debug --output-on-failure`; command-line examples below use Release paths.

## 2. Check a hand-solvable greedy failure

```powershell
$app = '.\build\Release\seamcarve.exe'
& $app trace --grid data/demo/greedy_counterexample.txt --method dp
& $app trace --grid data/demo/greedy_counterexample.txt --method greedy
& $app trace --grid data/demo/greedy_counterexample.txt --method graph
```

The input is:

```text
1    2  100
1  100    2
100 100  1
```

Expect DP and graph cost **5**, greedy cost **102**. DP columns are `[1,2,2]`; greedy columns are `[0,0,0]`. Columns are zero-based. A graph result need not match DP coordinates on other inputs with ties, but its optimum cost must match on the same energy map. DP also prints its row-major cumulative table so you can trace the recurrence manually.

## 3. Check both seam directions and exact output size

```powershell
& $app resize --input data/demo/scene_1.png --output results/check_width.png --width 150 --method dp
& $app resize --input data/demo/scene_1.png --output results/check_height.png --height 100 --method dp
& $app resize --input data/demo/scene_1.png --output results/check_both.png --width 140 --height 96 --method dp --artifacts results/check_steps
& $app info --input results/check_width.png
& $app info --input results/check_height.png
& $app info --input results/check_both.png
```

The original is 160x112. Expect output sizes **150x112**, **160x100** and **140x96**. The third command performs **36 removals**: 20 vertical, then 16 horizontal. Inspect `results/check_steps/summary.json`: the first 20 entries have axis `v`, the next 16 have `h`; step dimensions describe the image before removal. Costs and paths come from freshly calculated current energy maps.

Use a new or empty artifacts directory for each recorded run. Rerunning into `results/check_steps` after it contains frames is deliberately rejected before writes; choose another directory name. Inputs must be outside the artifacts/comparison directory, and output paths must not alias the input or its JSON sidecar.

Open the saved images. Each `NNNN_seam.png` is the current image before removal with the selected seam highlighted; `NNNN_energy.png` shows its current Sobel energy; `NNNN_after.png` shows the image after deleting that seam. `all_seams.png` places all removed pixels back on the original coordinates. A correct coordinate map has exactly `originalWidth*originalHeight - targetWidth*targetHeight` colored pixels for this synthetic example; automated tests check exact coordinates as well.

Repeat with `--method greedy` and `--method graph`. Also test `--order horizontal-first`, which is expected to produce a potentially different final image. The controller must still reach exactly the same requested dimensions.

## 4. Compare all algorithms fairly

```powershell
& $app compare --input data/demo/scene_3.png --output-dir results/check_comparison --width 140 --height 96 --repeats 5 --steps
```

Check these files:

| File | What to check |
|---|---|
| `comparison.json` | Three methods, requested dimensions, five repetitions, median/min/max timings |
| `timings.csv` | 15 measured data rows: 3 methods x 5 repetitions, plus the header |
| `dp_vertical.png`, `greedy_vertical.png`, `graph_vertical.png` | All seams are connected on the same original image |
| Corresponding `_horizontal.png` files | Horizontal seam comparison on the same original image |
| `dp.png`, `greedy.png`, `graph.png` | Same target dimensions; inspect shape, lines, text and texture |
| `dp/`, `greedy/`, `graph/` | Every seam/energy/after frame, final output and all-seams overlay |

In JSON, `vertical_cost` must agree between DP and graph, and `horizontal_cost` must also agree. Greedy cost must be at least the optimum on these shared initial maps. Later steps can use different images after different tied or greedy choices, so do not compare their later costs as though the underlying map were the same.

The stage measurements include validation and horizontal transposition as documented. Plotting, disk I/O and frame mapping are outside timed stages. For formal comparisons use the no-frames measured runs that `compare` already performs; its separate optional frame run is not used for the median. Treat the viewer's one-run timings as illustrative.

## 5. Inspect the existing photographic evaluation

Start at `results/evaluation/SUMMARY.md`. It links the labeled comparison panels; each row folder contains raw outputs and first-seam overlays. `all_timings.csv` contains all 360 repeated full-resize measurements (12 images x 2 reductions x 3 methods x 5 repetitions). `scaling.csv` contains 24 method/orientation medians for single removals at square sizes 32, 64, 128 and 256. Exact benchmark input dimensions, hashes and environment are recorded in `inputs.json` and `environment.json`.

Compare the four photographs and difficult synthetic examples, especially diagonal lines. Lower energy need not mean a more pleasing final picture: greedy can preserve a particular structure better by chance, while DP and graph remain exact for their stated energy objective. Do not claim universal perceptual superiority.

To rerun these experiments, install/use Python with Pillow and run:

```powershell
python tools/evaluate.py --exe build/Release/seamcarve.exe --repeats 5
```

The default prepares copies with a maximum side of 96 for bounded repeated-resize tests; it does not alter the originals. `--max-side 128` increases those prepared sizes. The separate single-removal scaling experiment reaches 256. Record the actual sizes when reporting results. Larger repeated graph reductions may take substantially longer.

## 6. Check the interactive viewer

```powershell
& $pythonExe .\web\server.py --exe .\build\Release\seamcarve.exe --port 8765
```

At `http://127.0.0.1:8765`:

1. Click **Try sample image**, or upload `data/demo/scene_1.png`.
2. Set width **152**, height **108**, and select **Compare all three**.
3. Run. Expect three **152x108** results and **12 seams per method**.
4. Move through steps 1-8 (vertical), then 9-12 (horizontal).
5. Switch between **Seam before removal**, **Energy map before removal**, **After removal** and **All seams on the original**. Check stage dimensions and current-image overlays.
6. Select another algorithm, play/pause, move back/forward and download the final PNG.
7. Reset, select an unchanged target and run again. Expect the original dimensions and no removals.

If graph comparison rejects a large upload, use a smaller copy or choose DP/greedy; this explicit UI limit is not an algorithmic correctness failure. Sessions disappear when the server stops. Press Ctrl+C in its terminal to stop it.

## 7. Check rejected inputs

```powershell
& $app resize --input data/demo/scene_1.png --output results/invalid.png --width 0 --method dp
& $app resize --input data/demo/scene_1.png --output results/invalid.png --width 161 --method dp
& $app resize --input missing.png --output results/invalid.png --width 10 --method graph
& $app resize --input data/demo/scene_1.png --output results/invalid.png --method unknown
```

Each must print `Error: ...` and exit with code **1**. The automated tests additionally cover corrupted grids, extra tokens, duplicates/unknown arguments, wrong storage, invalid seams, output errors, very large costs and capacity overflow. Shrinking a dimension to one and leaving the target unchanged are valid cases, not errors.

## Common problems

- **PowerShell says scripts are disabled:** use the direct CMake/Python commands in [README.md](README.md). You do not need to change the execution policy. After the commands build successfully, `ctest --test-dir build -C Release --output-on-failure` reruns all tests without a `.ps1` file.
- **Compiler missing:** install/enable Visual Studio C++ desktop tools and rerun CMake. The checked environment used MSVC 19.44 with Visual Studio 2022.
- **Only one CTest test appears:** configure with an actual Python 3.10+ executable: `cmake -S . -B build -DPython3_EXECUTABLE="C:/path/to/python.exe"`. All three tests should be registered.
- **Python opens the Microsoft Store:** pass an actual runtime using `run_viewer.ps1 -Python "C:/path/to/python.exe"`. The provided script detects the installed bundled runtime on this machine.
- **Port in use:** run `run_viewer.ps1 -Port 8766` and use its printed URL.
- **Graph slow:** exact max-flow has higher overhead than specialized DP. Reduce input size or seam count for demos, and preserve/report any size limits in evaluation.
- **Different tied paths:** compare optimum energy and seam validity; identical coordinates are not required.
