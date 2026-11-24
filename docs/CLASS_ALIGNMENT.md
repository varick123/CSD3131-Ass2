# Alignment with lecture and tutorial material

All ten lecture PDFs and five tutorial PDFs in the course workspace were reviewed for the assignment plan. Page references below are physical PDF pages, not necessarily printed slide numbers. The assignment requirements come from `Assignment 2.pdf`, pp. 1-4; the assessment categories come from the complete one-page `Rubrics.pdf`. Architecture choices and the corrected graph boundary model are implementation decisions.

| Course document | Pages | Application to this implementation |
|---|---|---|
| `CSD3130 L01 (stable matching, big-O).pdf` | 2, 8-10, 19-21, 23-29, 32-34 | Precise problem formulation, termination and proof, polynomial versus exponential growth; used in the seam definition and counting argument. |
| `CSD3130 L02 (BigO).pdf` | 6-7, 11-17, 22-31 | Input-size definition and operation counting; underpins the stage and whole-resize bounds in `ANALYSIS.md`. |
| `CSD3130 L03 Recursion (revised-2).pdf` | 2-13, 24-26, 31-35, 43-58 | Base cases, dependencies, repeated work and recurrences; used to explain why endpoint tabulation replaces repeated recursive exploration. |
| `CSD3130 L04  (Brute-Force, Divide and Conquer, Sorting).pdf` | 2, 12-32, 87-88 | Exhaustive enumeration as an independent tiny-input oracle; analyzing actual work and storage rather than assuming recursion is faster. |
| `CSD3130 L05 (Backtracking, Greedy, Dynamic Programming).pdf` | 5-23, 42-45, 49-66 | Problem-specific greedy reasoning and counterexamples; optimal substructure, overlapping subproblems, tabulation and storage tradeoffs in `greedy.cpp` and `dp.cpp`. |
| `8. More D&C_v3.pdf` | 4, 34, 37-38, 52 | Split/solve/combine and recurrence reasoning; supports the fixed-kernel convolution discussion and its output-size lower bound. |
| `9. DP1_v4.pdf` | 3-7, 9, 12, 19, 25 | Bottom-up computation, grid states and SRTBOT: define subproblems, recurrence, order, base cases, original answer and time. Applied directly to `find_dp`. |
| `10. DP2_v3.pdf` | 4, 6 | Auxiliary reconstruction information and dependency-sensitive memory compression; explains two cost rows plus retained parent offsets. |
| `11. Graph 1_v2.pdf` | 5, 15, 21, 32 | Adjacency lists, BFS and predecessor/path information; used in flow levels, explicit blocking-path stacks and residual reachability. |
| `12. Graph 2_v2.pdf` | 40-55, 56-61 | Flow constraints, residual correction, Ford-Fulkerson/Edmonds-Karp, then Dinic levels and blocking flow. `flow.cpp` implements Dinic; Edmonds-Karp is contextual material, not an implemented alternative. |
| `CSD3130 T01.pdf` | 1 | Proof/counterexample construction and growth ranking. |
| `CSD3130 T02.pdf` | 1 | Dominant terms, scaling observations and runtime crossovers; guides fair measurement. |
| `CSD3130 T03.1.pdf` | 1 | Loop and recurrence counting; supports complexity derivations. |
| `CSD3130 T04.pdf` | 1 | Worked traces; motivates small cumulative-table and path examples. |
| `CSD3130 T05.pdf` | 1 | Backtracking, rod-cutting DP and problem-specific greedy reasoning; supports the independent oracle and DP explanation. |

Stable matching, sorting, closest-pair geometry, spanning trees and bipartite matching provide course context but are not required seam-carving components. No implementation of those unrelated algorithms is claimed. Standard shortest-path methods are useful conceptual comparisons; they do not replace the required max-flow/min-cut implementation.

Use `O`, `Omega` and `Theta` as mathematical bounds, and state the analyzed case separately. For example, the smaller greedy choice loop does not make its validated public API sublinear in the energy-grid size. Likewise, reduced DP cost-buffer storage does not remove reconstruction storage. The [technical analysis](ANALYSIS.md) connects these distinctions to the actual source; the [evaluation summary](../results/evaluation/SUMMARY.md) records observations separately.
