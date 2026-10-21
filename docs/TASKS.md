# Team work allocation

Choose owners together; do not assume one row equals one team member. Agree interface changes before merging. Everyone should review another module and understand the complete solution for the quiz.

| Workstream | Owner | Implementation work | Evidence before merging |
|---|---|---|---|
| Image/energy | To assign | I/O adapter, normalization, energy, border/numeric policies | Hand-calculated energy and format tests |
| Dynamic programming | To assign | State, table order, parents, reconstruction | Independent optimum tests and proof |
| Greedy | To assign | Explicit local rule and tie handling | Legal seams, failure example and comparison |
| Graph cut | To assign | Graph/capacities, max-flow, residual cut recovery | Flow/cut tests, boundaries and optimum agreement |
| Resize/integration | To assign | Removal, transpose, repeated targets, CLI | Survivor-order tests, both axes and validation |
| Evaluation/documentation | To assign | Dataset, timing harness, analysis, report and video | Reproducible evidence and reviewed explanations |

Suggested merge sequence:

1. Confirm the shared data contracts and choose an image library.
2. Integrate image loading, energy and a one-seam DP path.
3. Add removal and horizontal/repeated resizing.
4. Integrate greedy and graph cut through the same pipeline.
5. Run independent tests and fair comparisons; write the analysis alongside results.
6. Add bonus visualization/interaction/automation if the required work is stable.
7. Review the report, rehearse within the limits, record and check required links/uploads.

Keep each change reviewable. A merge should state what was implemented, what was tested and any known limitations. Do not describe the starter's smoke tests as evidence that unfinished algorithms work.
