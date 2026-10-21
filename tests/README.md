# Tests to develop

The two current CTest entries only check that the executable launches and reports its starter status. They do not test energy, seam correctness, resizing, performance or image quality. Add real tests when implementing each module.

Suggested acceptance tests:

- Energy: constant image, hand-computed positive/negative gradients and explicit border behavior.
- DP: hand grids, legal traceback, ties and comparison with an independently written exhaustive tiny-grid seam oracle.
- Greedy: connectivity and a counterexample where its cost exceeds the optimum.
- Graph cut: residual cancellation, min-cut decoding, both edge-column optima, ties and agreement with an independent optimum.
- Removal: exactly one pixel per row, survivor order and horizontal transpose equivalence.
- Controller: unchanged target, both dimensions, shrink-to-one, invalid targets and fresh energy after every removal.
- Experiments: equal originals, targets, energy and axis policy; reproducible timings and side-by-side outputs.

`fixtures/energy_3x3.txt` contains height, width and row-major sample energies. Work out the expected result independently before coding a test. `data/tiny_gradient.ppm` is an RGB image fixture for future I/O and energy tests, not a photographic evaluation dataset.
