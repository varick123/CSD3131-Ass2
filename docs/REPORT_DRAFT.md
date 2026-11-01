# Image Resizing by Seam Carving

**Working report draft: team review and final formatting remain outstanding.**

| Team information | To be completed by the team |
|---|---|
| Team ID | **PENDING: enter the actual team ID** |
| Members and SIT IDs | **PENDING: enter every member's name and SIT ID** |
| GitHub repository | [CSD3131-Ass2 implementation branch](https://github.com/varick123/CSD3131-Ass2/tree/varick) - implementation published on `varick`; **verify assessor access and the final submission branch before submission** |
| Recorded presentation | **PENDING: record, upload and insert the accessible video URL** |

## 1. Objective and design

Seam carving changes image dimensions by removing connected paths whose pixels have low importance under an energy function. Unlike uniform scaling, it can remove background while retaining more of a salient object. Its success depends on whether the energy function reflects the content people want to preserve. Avidan and Shamir introduced this content-aware resizing approach [1].

This project implements dynamic programming (DP), a local greedy algorithm and genuine graph cut using max-flow/min-cut. Each supports vertical and horizontal removal. All use the same image representation, Sobel energy, connectivity constraints and resize controller, making their objective values and timings comparable. The implementation shrinks images; seam insertion is outside the implemented scope.

The C++17 core has no OpenCV dependency. The vendored stb libraries handle image decoding/encoding only [4]; no content-aware resizing routine is called. [Source architecture](../src/) separates image codecs, energy generation, seam search, residual flow, validation and resizing. [The course alignment](CLASS_ALIGNMENT.md) records the relevant DP, greedy, graph, complexity and tutorial material.

## 2. Image model and energy

Let the current height and width be `H` and `W`. A vertical seam is `s[0..H-1]` with `0 <= s[r] < W` and `|s[r]-s[r-1]| <= 1`. Its fixed-map cost is `sum(E[r,s[r]])`. Horizontal seams use transposed axes.

[Image input](../src/image_io.cpp) becomes 8-bit RGB; alpha is composited onto white, while HDR and 16-bit images are rejected explicitly. [Energy generation](../src/energy.cpp) computes grayscale as

```text
I = (299*R + 587*G + 114*B + 500) / 1000
E = abs(Gx) + abs(Gy)
```

The division is integer division. `Gx` and `Gy` are signed 3x3 Sobel convolutions with replicated outer borders. Retaining signed gradients avoids clipping negative differences. Nonnegative energies and cumulative costs use checked signed 64-bit arithmetic. [Validation](../src/core.cpp) rejects inconsistent storage, invalid dimensions, negative energy, overflowing sums and disconnected seams.

[The resize loop](../src/resize.cpp) finds and removes one seam, recomputes energy from the changed pixels and repeats. It defaults to vertical-first removal and also supports horizontal-first. Horizontal processing transposes, invokes the vertical solver and transposes back. Both orders are valid but can produce different final images.

## 3. Exponential search and dynamic programming

For `W > 1`, restrict every row to two adjacent columns. Each of the `H` rows independently chooses either column. Every such binary sequence is a legal seam, giving at least `2^H` possibilities. A general upper bound is `W*3^(H-1)`, since each continuation has at most three choices. Exhaustive seam enumeration therefore scales exponentially with height.

[DP](../src/dp.cpp) defines `M[r,c]` as the cheapest seam prefix ending at `(r,c)`:

```text
M[0,c] = E[0,c]
M[r,c] = E[r,c] + min(M[r-1,p])
          over valid p in {c-1,c,c+1}
minimum seam cost = min(M[H-1,c])
```

Rows are processed from top to bottom. For correctness, the first row is immediate. Assuming preceding-row costs are optimal, any seam to `(r,c)` must come through one legal predecessor. Replacing an expensive prefix by the optimum prefix to that predecessor preserves validity. The recurrence considers every legal final transition and selects the least expensive. The final-row minimum considers every endpoint; recorded predecessors reconstruct a seam attaining it.

Only `H*W` endpoint states are needed despite exponentially many complete seams. The implementation keeps two cost rows and one signed byte of predecessor offset per pixel. Ascending candidate order and strict comparisons select the leftmost predecessor and endpoint on ties. An optional cumulative table supports worked traces. This proves minimum cost for one current seam, not global optimality of all future removals or perceptual quality.

## 4. Greedy alternative

[Greedy search](../src/greedy.cpp) selects the cheapest first-row pixel and then the cheapest legal neighbor in each next row. It uses leftmost ties and preserves connectivity. It does not look ahead or try every starting column.

```text
  1    2  100
  1  100    2
100  100    1
```

Here greedy follows `[0,0,0]`, costing `102`; DP follows `[1,2,2]`, costing `5`. Cheap initial choices can force expensive later choices, so the greedy-choice property does not hold generally. Greedy remains useful as a simple baseline whose actual visual and runtime behavior can be measured.

## 5. Graph-cut formulation and implementation

[Graph cut](../src/graph_cut.cpp) constructs a directed capacity graph and uses the self-written [Dinic solver](../src/flow.cpp). The assignment's literal sketch forces actual last-column pixels to the sink side, which excludes that column when decoding the rightmost source-side pixel. Boundary vertices resolve this endpoint issue and include width-one energy maps.

Create `W+1` boundary vertices `b[r,0..W]` per row, plus source and sink. Add:

- Source to `b[r,0]`, and `b[r,W]` to sink, with capacity `INF`.
- `b[r,c] -> b[r,c+1]` with capacity `E[r,c]` for every pixel.
- Structural backward edges `b[r,c+1] -> b[r,c]` with capacity `INF`.
- Diagonal edges `b[r,j] -> b[r-1,j-1]` and `b[r,j] -> b[r+1,j-1]` with capacity `INF` where rows exist.

Set `INF = sum(E)+1`, rejecting overflow. A legal seam costs at most `sum(E)`, so an optimum cut never crosses a constraint edge. Backward constraints force each row's source side to be a prefix. Its greatest boundary index lies in `0..W-1` and identifies the removed pixel. Exactly one finite edge crosses each row, charging that pixel's energy. Diagonal constraints force adjacent prefix endpoints to differ by at most one. Conversely, every legal seam defines such prefixes, so finite cuts and seams have equal costs. The max-flow/min-cut theorem therefore establishes optimality for the shared energy objective.

Dinic builds BFS levels and augments blocking paths using current-edge pointers and explicit stacks. Each original edge has a separate zero-capacity reverse residual partner; these must not be confused with structural backward edges. Reverse residuals permit cancellation of earlier flow. After max-flow, positive-residual reachability identifies the source side. Decoding checks connectivity and equality between flow value and recovered seam energy. DP and graph cut must agree on optimum cost on the same map, although tied paths may differ. Graph-cut seam models and forward-energy alternatives also appear in Rubinstein et al. [2]; this implementation uses the same backward Sobel objective as DP.

## 6. Complexity and improvement discussion

Let `N = HW`. The graph has `V = H(W+1)+2` vertices and `A = 2H+2HW+2(H-1)W` original directed edges; residual partners add a constant factor.

| Stage | Time | Additional storage |
|---|---|---|
| Sobel energy | `Theta(N)` | `Theta(N)` |
| DP, including reconstruction | `Theta(N)` | `Theta(W)` costs, `Theta(N)` parents |
| Greedy decision loop | `Theta(W+H)` | `Theta(H)` seam |
| Public validated greedy solver | `Theta(N)` | `Theta(H)` seam |
| Graph construction | `Theta(V+A)` | `Theta(V+A)` |
| Dinic, general worst case | `O(V^2*A)` | `O(V+A)` |
| Residual decoding | `O(V+A)` | `O(V)` |
| Transpose or removal by copying | `Theta(N)` | `Theta(N)` |

Greedy's public solver validates the entire energy grid; its smaller choice loop does not make that API sublinear. Full DP and greedy removal also compute energy and copy survivors, giving `Theta(N)` work each. DP's complete storage remains `Theta(N)` despite rolling cost rows because traceback parents remain. An optional cumulative table adds another `Theta(N)` buffer. Dinic's general bound becomes `O(N^3)` here; measured runtimes do not establish that every input attains it.

For `k` vertical removals, the DP/greedy pipeline sums changing image areas:

```text
Theta(sum(t=0..k-1) H*(W-t))
= Theta(H*(kW-k(k-1)/2)), k >= 1.
```

Add the corresponding horizontal sum using the resulting width. Graph resizing sums construction, flow and decoding over current dimensions. Stored seam history adds the sum of oriented seam lengths; saved frame files can dominate storage.

For Item 2(b), fixed 3x3 Sobel performs constant work per output pixel, so direct convolution is `Theta(N)`. Producing `N` outputs requires `Omega(N)` work; recursive partitioning cannot improve this asymptotic bound. Tiling may improve locality or enable threads, but requires one-pixel halos and correct outer borders. Treating internal tile edges as image borders changes results. The current algorithms are sequential. Parallel energy pixels and cells within one DP row are possible, while successive rows and seam removals have dependencies. Incremental energy updates and forward energy are future experiments requiring correctness checks; no unmeasured speedup is claimed. [ANALYSIS.md](ANALYSIS.md) supplies further derivations.

## 7. Correctness evidence

[Core tests](../tests/test_algorithms.cpp) pass 43,477 assertions. Independent recursive enumeration checks 1,689 tiny energy maps, including every binary map of shapes 1..3 by 1..3 and seeded maps through 5x5. DP cumulative prefixes and DP/graph optima are checked against enumeration, not only against each other. Tests include both border optima, one-row/column maps, ties, the greedy counterexample, invalid input and integer limits. Exhaustive cuts check 205 general flow graphs, including cancellation, parallel, antiparallel and self edges. Independent convolution checks 207 images.

[CLI integration tests](../tests/test_cli.py) independently replay resizing from emitted seams, recompute current energy, decode PNG scanlines, verify survivor pixels and check original-coordinate overlays. They cover both axis orders, unchanged targets, shrink-to-one, artifacts, CSV/JSON comparisons, malformed arguments, Unicode paths, grayscale/alpha inputs, unsupported depths and input-preserving output validation. The checked Windows run passed 8,624 integration assertions over 88 commands and 38 resize sequences. [HTTP tests](../tests/test_viewer.py) exercise six cases and 130 requests against the real executable, including downloads, every frame stage, invalid settings and the pre-decode image limit. All three CTest suites passed in local Release and Debug builds. These tests support specified cases; they do not prove absence of every defect or establish performance on arbitrary images.

## 8. Reproducible evaluation

[The benchmark](../tools/evaluate.py) uses eight procedural patterns and four attributed photographs [3]. Aspect-preserving preparation caps the long side at 96 pixels: synthetic inputs become 96x67, astronaut 96x96 and the other photographs 96x64. Common targets reduce both dimensions by nominally 10% or 25%, rounded to integers. All methods start from separate copies, use vertical-first order and share energy/border rules.

The recorded environment is Windows 11, an AMD64 processor with 16 logical CPUs, C++17 MSVC Release `/O2`, and one algorithm thread. Each case has one warm-up and five measured runs. `steady_clock` measures energy, search and removal stages; disk I/O and visualization are excluded. Raw repetitions, environment, prepared dimensions and input hashes are retained in [evaluation files](../results/evaluation/). Medians below are whole-resize stage sums in milliseconds.

| Photograph, 25% reduction | Prepared to target | DP ms | Greedy ms | Graph ms | Greedy/DP first vertical cost |
|---|---|---:|---:|---:|---:|
| [Astronaut](../results/evaluation/astronaut_25/comparison.png) | 96x96 to 72x72 | 4.843 | 4.052 | 1724.099 | 3.918 |
| [Chelsea](../results/evaluation/chelsea_25/comparison.png) | 96x64 to 72x48 | 2.780 | 2.835 | 753.472 | 1.819 |
| [Coffee](../results/evaluation/coffee_25/comparison.png) | 96x64 to 72x48 | 2.908 | 2.326 | 719.183 | 2.148 |
| [Rocket](../results/evaluation/rocket_25/comparison.png) | 96x64 to 72x48 | 3.070 | 2.484 | 357.264 | 3.084 |

DP and graph agreed on all 48 reported shared-original-map orientation costs across 24 resize cases. These repeat first-seam comparisons for two reduction amounts and are not 48 independent images. Greedy's first vertical cost was higher in 18 of 24 cases. This compares one common map; later maps diverge after different removals.

Greedy was usually faster in these small cases, but Chelsea at 25% was slightly slower than DP. Graph's general machinery was substantially slower than the specialized DP solver. [Single-vertical-removal scaling](../results/evaluation/scaling.csv) measured DP at 0.0113, 0.0534, 0.1857 and 0.7438 ms for 32, 64, 128 and 256 square images; graph took 0.9423, 6.3842, 61.5618 and 2103.6507 ms. These are bounded workload observations, not universal speed ratios or a fitted complexity proof.

Visual inspection also limits any claim of superiority. At 25%, greedy narrows the astronaut's head more visibly and deforms the coffee cup differently from DP/graph. All methods alter narrow structures in rocket. On [synthetic diagonals](../results/evaluation/scene_3_25/comparison.png), DP and graph introduce pronounced kinks while greedy retains a straighter X despite a much larger first-seam energy ratio, 41.700. The energy objective does not encode line straightness or object identity. Scaling is included as a labeled visual baseline, and can preserve geometric regularity better on some patterns. These observations are qualitative and were not a blinded user study.

## 9. Features, limitations and conclusion

Per-removal energy, seam and output frames plus original-coordinate all-seams overlays support inspection. A local browser interface provides image selection, target controls, method/order selection, playback and export. A [workflow](../.github/workflows/build-test.yml) defines Windows/Linux builds, tests and downloadable executable artifacts; remote execution remains dependent on publishing the repository and checking its runs.

The evaluation uses small prepared images, a limited dataset, one energy function and one measured axis order. There is no semantic object protection, global sequence optimization, insertion or validated parallel implementation. Repeated low-energy removals can still distort important content. DP is the practical exact solver for this objective in the measured workload; graph cut independently reaches the same single-seam optimum at higher cost, and greedy trades optimality for simpler decisions. Final method choice should consider both measured efficiency and visible artifacts.

## References

1. Avidan, S. and Shamir, A. (2007). [Seam Carving for Content-Aware Image Resizing](https://www.merl.com/publications/TR2007-087). ACM Transactions on Graphics, 26(3). Background seam model and energy-based resizing.
2. Rubinstein, M., Shamir, A. and Avidan, S. (2008). [Improved Seam Carving for Video Retargeting](https://www.merl.com/publications/TR2008-064). ACM Transactions on Graphics, 27(3). Related graph-cut and forward-energy approach; video retargeting is outside this project.
3. [scikit-image 0.25 data documentation](https://scikit-image.org/docs/0.25.x/api/skimage.data.html). Photograph sources and status; exact files and attributions are recorded in [data/ATTRIBUTION.md](../data/ATTRIBUTION.md).
4. Barrett, S. and contributors. [stb libraries](https://github.com/nothings/stb). Image codecs only; vendored notices are retained in [third_party](../third_party/).
5. Course assignment and rubric: `ass2/Assignment 2.pdf`, pp. 1-4, and `ass2/Rubrics.pdf`, p. 1. Lecture/tutorial page mapping: [CLASS_ALIGNMENT.md](CLASS_ALIGNMENT.md).
