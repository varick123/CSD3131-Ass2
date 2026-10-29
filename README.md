# Seam Carving Assignment 2

## Implementation stage: image resizing and CLI

This branch integrates the existing local implementation in tested stages. The application now supports PNG/JPEG/BMP/PPM loading, RGB normalization, vertical/horizontal resizing with fresh energy after every removal, DP/greedy/graph-cut methods, repeatable comparisons and step artifacts. A viewer, CI and evaluation documentation follow in subsequent commits.

- [Assignment requirements and rubric](docs/ASSIGNMENT_REQUIREMENTS.md)
- [Team review tasks](docs/TASKS.md)
- [Image provenance](data/ATTRIBUTION.md)
- Codec license: `third_party/LICENSE-stb.txt`

## Build and test

Use CMake 3.20+, a C++17 compiler and Python 3.10+ for integration tests. Set `-DPython3_EXECUTABLE=<installed-python-path>` if discovery fails.

```powershell
cmake -S . -B build
cmake --build build --config Release --parallel 4
ctest --test-dir build -C Release --output-on-failure
.\build\Release\seamcarve.exe resize --input data/demo/scene_1.png --output results/check.png --width 140 --height 96 --method dp
```

Expect two CTest suites: `algorithms` and `command_line`. Core checks use independent exhaustive oracles. CLI checks independently replay image removals and verify frame artifacts and invalid-input handling. On single-configuration generators the executable is `build/seamcarve` instead.
