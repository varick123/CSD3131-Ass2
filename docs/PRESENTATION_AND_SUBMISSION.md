# Presentation, submission and quiz preparation

This is a working guide. The report still needs actual team details, verified assessor access to the implementation branch, a recorded presentation URL and the team's final review. The implementation is published on `varick`; presentation upload, final team review and peer evaluation remain outstanding.

## Presentation limits and suggested sequence

The brief allows **at most 2 minutes per person and 12 minutes for the whole team**. Set each speaker's target below two minutes and include transitions and demonstration time inside the team limit. Divide topics according to the actual team size; the blocks below are topics, not an assumption of six members.

| Topic | Suggested content and evidence |
|---|---|
| Problem and energy | Original/target dimensions, legal connected seam, signed Sobel energy and repeated recomputation |
| DP | State, three-predecessor recurrence, base cases, traceback and one-seam optimality |
| Greedy | Trace the 102-versus-5 counterexample and explain why local choices can fail |
| Graph cut | Boundary vertices, finite energy edges, structural constraints, reverse residuals and cut decoding |
| Tests and measurements | Independent tiny-grid/cut oracles; a small timing table; explain common objective and fairness |
| Demonstration and limitations | Shrink both axes, inspect step frames and all-seam overlay, export; show a successful case and the diagonal failure case |

Use a rehearsed recording around 10-11 minutes if the actual speaker count permits that length. For a smaller team, reduce the overall target so each individual remains under two minutes. Display readable diagrams and a few comparisons; do not spend the recording scrolling through source listings. State that DP/graph optimize current seam energy and that this does not guarantee the visually best final picture.

## Reproducible demonstration commands

Run these in the project directory. On Windows, the executable path below matches a Visual Studio Release build; on a single-configuration Linux build use `./build/seamcarve`.

```powershell
cmake -S . -B build
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure
```

Show the small trace first, where results are predictable and readable:

```powershell
.\build\Release\seamcarve.exe trace --grid data/demo/greedy_counterexample.txt --method dp
.\build\Release\seamcarve.exe trace --grid data/demo/greedy_counterexample.txt --method greedy
.\build\Release\seamcarve.exe trace --grid data/demo/greedy_counterexample.txt --method graph
```

Expected costs are DP `5`, greedy `102`, graph `5`. DP also prints the cumulative table. Explain the connected columns rather than presenting the output as unexplained proof.

Use a bounded image for the live all-method comparison so graph-cut time stays manageable:

```powershell
.\build\Release\seamcarve.exe compare --input results/evaluation/inputs/coffee.png --output-dir results/presentation_coffee --width 72 --height 48 --repeats 2 --steps
```

This creates method outputs, original-map overlays, measurements and per-step artifacts. Inspect `comparison.json` and `timings.csv`, and show a method's `all_seams.png` alongside step frames. Timing repetitions during this short demonstration are separate from the five-run evaluation reported in the draft.

Use new/empty method artifact directories when rehearsing another recorded run; existing nonempty frame directories are rejected to prevent mixed sequences. Change the output-directory name for a new rehearsal.

To demonstrate horizontal-first reuse and exported frames:

```powershell
.\build\Release\seamcarve.exe resize --input results/evaluation/inputs/astronaut.png --output results/presentation_astronaut.png --width 84 --height 84 --method dp --order horizontal-first --artifacts results/presentation_astronaut_frames
```

For the interactive demonstration, start the local viewer:

```powershell
.\run_viewer.ps1
```

Use the URL printed by the launcher. Load a small input, select a method and order, enter smaller target dimensions, run, inspect energy/seam/output frames, play or step through the sequence and export the result. Reset to the original before changing comparisons. Do not present prerecorded result files as a new successful execution. Keep a short prepared recording as a presentation fallback if a live window fails.

Include [diagonal scene 3 at 25%](../results/evaluation/scene_3_25/comparison.png) to show that DP/graph's lower energy can still kink straight features. Contrast it with [coffee](../results/evaluation/coffee_25/comparison.png) or [astronaut](../results/evaluation/astronaut_25/comparison.png), where greedy distorts salient shapes differently. This makes the quality discussion evidence-based.

## Complete the report and repository

- Replace every pending field in [REPORT_DRAFT.md](REPORT_DRAFT.md) with the real team ID, every member's name/SIT ID, GitHub URL and video URL.
- Review all algorithms and results as a team. Keep the implemented conventions, source, graphs, tables and spoken explanation consistent.
- Select readable figures from [evaluation outputs](../results/evaluation/) and caption input/target dimensions, method, energy and axis order. Retain raw measurements and image attributions.
- Reproduce a Release build and tests from a fresh checkout on another team member's machine. Record the actual compiler/platform if results are rerun.
- Confirm the assessor can access the intended implementation branch (`varick` currently), retain third-party notices and check the workflow's remote status before claiming CI passed remotely.
- Record the presentation, upload it to an accessible online platform, test playback without relying on the uploader's signed-in account and add the verified link to the report.
- Convert the reviewed draft into the required final report format after checking XSite for any additional upload rules. The supplied brief sets no page limit or mandatory file format.
- Complete the course's peer-evaluation process. Its procedure is not supplied in the one-page rubric; use the actual course instructions.

## Submission checklist

The printed deadline is **Friday 20 November, 23:59, Week 12**. The current course context places this on **20 November 2026, Singapore time**. Leave time to verify the submitted files after upload.

| XSite destination | Content |
|---|---|
| Assignment 2 Report Only | Final reviewed report with team/member IDs, Item 2 answers, algorithm/design/implementation, evaluation, comparisons/discussion, GitHub and presentation links |
| Assignment 2 Other Materials | Slides if used; selected test inputs/outputs or other supporting materials if included |

Keep the ordinary submission package deliberate: final report, optional slides and intended supporting material. Exclude local build directories, caches, working plans, temporary viewer sessions and unreviewed drafts. Test that links work and uploaded files open correctly; this guide does not itself perform those actions.

The rubric assigns Design 20, Implementation 20, Analysis 20, Documentation 20, Presentation 15 and Peer Evaluation 5 marks. The bonus is up to 10 for implementation quality involving step visualization, interaction and CI/CD; no separate mark allocation per feature is given. Demonstrate only features that work in the submitted version.

## Team quiz study

The quiz is **Tuesday 24 November during tutorial, Week 13**, using the current course context of 2026. It is closed-book, team-based and contains 10-20 MCQs based on the submitted code/report. Relevant extracts are supplied. Prepare to work using those extracts and team discussion without external materials or tools.

Every member should explain and trace:

1. Seam bounds/connectivity; why two adjacent columns already give `2^H` legal paths.
2. DP state, recurrence, base/boundary cases, processing order, parent offsets and tie handling.
3. The greedy 102-versus-5 example and the distinction between local decisions and global single-seam optimality.
4. Why the graph has `W+1` boundaries; how structural/diagonal constraints enforce a seam; why `INF=sum(E)+1` is sufficient.
5. Structural backward edges versus reverse residual partners; how cancellation works; BFS levels, blocking flow and source-side recovery.
6. Signed Sobel, grayscale rounding, replicated borders, recomputation after every removal and transpose reuse.
7. Why rolling costs retain `Theta(HW)` traceback storage; why validation makes the public greedy solver linear in pixels; single-seam versus full-resize costs.
8. Why fixed 3x3 divide-and-conquer convolution cannot beat linear output work; where threading could help and where dependencies remain.
9. The actual experiment controls, stage timing exclusions, small dataset limitations and why optimum energy does not prove visual superiority.
10. Error/overflow handling, independent test oracles, graph boundary tests and interpretation of emitted JSON/artifacts.

Practice by having each person explain another member's module and manually trace a tiny example. The tentative moderation multipliers in the brief are 1.00 for 80-100%, 0.95 for 60-79%, 0.85 for 40-59%, and 0.70 below 40%. Understanding all submitted parts matters in addition to dividing implementation work.
