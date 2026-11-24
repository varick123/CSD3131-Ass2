# Algorithm analysis

This document describes the implemented C++17 core and supplies the reasoning required by Assignment 2, Items 2 and 5. It is technical report material, not a completed team report. Measured comparisons are recorded in the [evaluation summary](../results/evaluation/SUMMARY.md); theoretical bounds below are not measured timings.

## Model and implementation conventions

Let the current image have height `H`, width `W`, and `N = H*W`. A vertical seam contains one column `s[r]` per row, with `0 <= s[r] < W` and `abs(s[r]-s[r-1]) <= 1`. Its cost on a fixed energy map is `sum(E[r,s[r]])`.

[image_io.cpp](../src/image_io.cpp) uses stb only for image codecs. Inputs become three-channel, 8-bit RGB; transparency is composited onto white. HDR and 16-bit inputs are rejected. There is no OpenCV dependency. [energy.cpp](../src/energy.cpp) computes integer grayscale as `(299*R + 587*G + 114*B + 500)/1000`, applies signed 3x3 Sobel kernels with replicated outer borders, and sets `E = abs(Gx) + abs(Gy)`. It preserves negative gradients until the absolute-value operation. All algorithms use these same nonnegative integer energies.

[core.cpp](../src/core.cpp) validates dimensions, storage, nonnegative costs, checked 64-bit sums and seam connectivity. [resize.cpp](../src/resize.cpp) removes one seam, recomputes the current energy map and repeats. Horizontal removal transposes image axes, uses the same vertical solver, and transposes back. The default axis order is vertical first; horizontal first is also supported. Order can change the final image.

## Item 2(a)(i): exponentially many seams

For `W > 1`, restrict every row to two adjacent columns. Independently choose either column in each of the `H` rows. Every binary sequence is legal because each successive column difference is at most one. These choices give `2^H` distinct seams, so the total count is at least exponential in height. An upper bound is `W*3^(H-1)`: choose a start, then at most three continuations per row. Border restrictions can reduce this upper bound.

## Item 2(a)(ii): dynamic programming

[dp.cpp](../src/dp.cpp) defines `M[r,c]` as the minimum energy of a valid seam prefix from the top row to `(r,c)`:

```text
M[0,c] = E[0,c]
M[r,c] = E[r,c] + min M[r-1,p]
          for valid p in {c-1,c,c+1}
answer = min M[H-1,c]
```

States are evaluated from top to bottom. Every state has at most three predecessor choices. The implementation stores two cost rows and a signed one-byte predecessor offset per pixel, then reconstructs from the cheapest final endpoint. Strict comparisons and ascending candidate indices choose the leftmost predecessor and final endpoint on ties. An optional output parameter retains the full cumulative table for inspection.

Correctness follows by induction on rows. At row zero the only prefix ending at a pixel is that pixel. Assume all preceding-row prefix costs are optimal. Every seam ending at `(r,c)` reaches it from a legal predecessor. If its prefix were more expensive than the best prefix to that predecessor, replacing it would give a cheaper valid seam to `(r,c)`. Thus the recurrence examines every possible final transition and selects the optimum. The final-row minimum considers every endpoint; the stored parents reconstruct a seam attaining its value. Overlapping endpoint subproblems explain why tabulation avoids exponential enumeration.

This proves optimality for one seam on the current map. It does not prove that a sequence of individually optimal removals produces the globally best final image.

## Greedy comparison

[greedy.cpp](../src/greedy.cpp) chooses the cheapest first-row pixel, then the cheapest legal neighbor in each next row. It uses the same leftmost tie rule and never selects disconnected row minima. Consider:

```text
  1    2  100
  1  100    2
100  100    1
```

Greedy chooses columns `[0,0,0]`, costing `102`. DP chooses `[1,2,2]`, costing `5`. A locally cheap choice can restrict later choices unfavorably; the greedy-choice property does not hold generally. Photographs are still needed to assess visible differences. A lower seam energy is an objective value, not a complete perceptual-quality measure.

## Graph cut and its boundary correction

[graph_cut.cpp](../src/graph_cut.cpp) uses a true minimum s-t cut, solved by the self-written Dinic implementation in [flow.cpp](../src/flow.cpp). The brief's literal first/last-pixel terminal construction excludes the final column from its decoding. Boundary vertices include every removable pixel, including width-one maps.

For each row create `b[r,0..W]`. Add source-to-`b[r,0]` and `b[r,W]`-to-sink edges of capacity `INF`. For each pixel add `b[r,c] -> b[r,c+1]` of capacity `E[r,c]`, and its structural backward edge of capacity `INF`. For `j=1..W`, add `b[r,j] -> b[r-1,j-1]` and `b[r,j] -> b[r+1,j-1]` at `INF`, where rows exist. Set `INF = sum(E)+1`, checking overflow before construction.

A legal seam gives a finite cut costing at most `sum(E)`, so an optimum cannot cross an `INF` edge. Structural backward edges force each row's source-side vertices to form a prefix. Terminal constraints make its greatest index `s[r]` lie in `0..W-1`. Exactly one finite edge crosses that row's cut, costing `E[r,s[r]]`. Diagonal constraints imply `s[r] <= s[r+1]+1` and its converse, hence seam connectivity. Conversely, every legal seam defines these prefixes without cutting a constraint edge. Finite cuts therefore correspond to legal seams with equal cost; the max-flow/min-cut theorem gives an optimal seam.

Dinic repeatedly builds BFS levels and finds blocking paths with explicit stacks and current-edge pointers. Each original edge has its own zero-capacity reverse residual partner, separate from structural backward edges. Reverse residuals allow earlier flow to be cancelled. After max-flow, positive-residual reachability identifies the source side. Decoding checks the prefix constraints, seam validity and equality of flow and recovered energy. DP and graph cut must agree on optimum cost for a shared map; tied paths can differ.

## Item 2(b): divide and conquer for Sobel

A fixed 3x3 filter performs constant work per output pixel, giving `Theta(HW)` work. Writing `HW` outputs already requires `Omega(HW)` work. Recursive image partitioning cannot improve this asymptotic bound. Tiling could improve cache behavior or permit parallel execution, but each tile needs a one-pixel halo. Padding internal tile edges as though they were image borders changes the answer. Overhead and duplicated halo work can cancel practical gains. Large-kernel FFT techniques use a different cost model and do not establish an improvement for this fixed kernel. The current implementation is sequential; no parallel speedup is claimed.

## Time and space costs

Here `A` counts original directed graph edges, including structural edges and excluding residual partners. The boundary graph has `V = H*(W+1)+2` and `A = 2H + 2HW + 2(H-1)W`; both are `Theta(N)`.

| Operation | Time | Additional storage |
|---|---|---|
| Energy generation / full energy validation | `Theta(N)` each | Generation `Theta(N)`; validation `O(1)` |
| DP and reconstruction | `Theta(N)` | Two rows `Theta(W)`, parents `Theta(N)`, output `Theta(H)` |
| Greedy choices alone | `Theta(W+H)` | Output `Theta(H)` |
| Public greedy function, including validation | `Theta(N)` | Output `Theta(H)` |
| Graph construction | `Theta(V+A) = Theta(N)` | `Theta(V+A)` |
| Dinic, general worst-case bound | `O(V^2*A)` | `O(V+A)`, including explicit stacks |
| Residual reachability and decoding | `O(V+A)` | `O(V)` |
| Image transpose or seam removal by copying | `Theta(N)` | `Theta(N)` |

The public greedy API calls full-map validation before selection and again through seam validation; it is therefore `Theta(N)`, even though its decision loop is smaller. Every baseline removal also generates energy and copies surviving pixels, making complete greedy and DP removal `Theta(N)`. Rolling costs do not make the complete DP solver `O(W)` space because it retains parents and the energy map. Requesting its optional cumulative table adds another `Theta(N)` buffer.

Dinic's general bound simplifies to `O(N^3)` here. This is a safe worst-case bound, not a fitted runtime prediction. Graph build statistics include validation/construction; flow statistics time max-flow alone. Residual decoding is included in the enclosing search time. Edge statistics exclude residual partners.

For `k >= 1` vertical removals, DP and greedy pipeline work is `Theta(H*(kW-k(k-1)/2))`, the sum of the shrinking image areas. Add the corresponding horizontal sum using the width reached after vertical removal. Graph resizing sums its construction, flow and decoding costs over those changing dimensions. Saving every seam record adds `O(kH)` for vertical history, or the sum of oriented seam lengths for mixed axes; frame storage can be much larger.

The automated tests establish optimum costs on tiny grids by independent enumeration, checking DP and graph against that oracle rather than each other alone. General flow tests exercise cancellation and antiparallel edges. The recorded performance experiments use common inputs, objective, dimensions and axis order; their measured evidence is distinguished from the theoretical bounds above.
