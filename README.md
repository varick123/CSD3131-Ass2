# Seam Carving Assignment 2

## Implementation stage: core algorithms

This branch integrates the existing local implementation in tested stages. The core library now implements RGB image/energy validation, signed Sobel energy, dynamic programming, greedy seams, residual max-flow/min-cut, transposition and seam removal. Image codecs, resize CLI and viewer will follow in subsequent commits.

The implementation uses `src/seam.h`; it replaces the starter interfaces. Algorithms share the same integer energy costs and image policies.

- [Assignment requirements and rubric](docs/ASSIGNMENT_REQUIREMENTS.md)
- [Team review tasks](docs/TASKS.md)

## Build and test

```powershell
cmake -S . -B build
cmake --build build --config Release --parallel 4
ctest --test-dir build -C Release --output-on-failure
```

The `algorithms` suite checks seam costs against exhaustive enumeration, max-flow against exhaustive cuts, Sobel against independent convolution, seam validation and removal. This stage has no application executable yet.
