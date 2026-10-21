# Assignment 2 - Seam Carving Starter

**Status: scaffold only. No seam-carving algorithm or image-resizing pipeline is implemented yet.** The executable builds and reports that status. Each module has a shared interface and a TODO that throws `NotImplemented`; no placeholder returns a fake resized image or optimal seam.

This repository is a shared starting point for the team to implement, test, evaluate and explain the assignment together. Owners are to be agreed in [docs/TASKS.md](docs/TASKS.md).

## Assignment requirements, summarized

Source: the course's **Assignment 2.pdf**, pp. 1-4, and **Rubrics.pdf**, p. 1. Keep those supplied documents as the authority. The brief permits any language and third-party image/math libraries, including image I/O and gradient/Sobel operations. **Do not use an existing content-aware resizing function.** C++17 is this starter's proposed team choice, not a mandated language.

### Required implementation

1. Calculate an image energy/disruption map from pixel gradients or a suitable filter such as Sobel.
2. Implement **dynamic programming** to find a minimum cumulative-energy seam in **both vertical and horizontal** directions.
3. Remove the selected seam, **recalculate energy**, then repeat until the requested dimensions are reached.
4. Implement a **greedy** alternative using legal local, row-by-row choices.
5. Implement **graph cut using min-cut/max-flow**, including residual-graph source-side recovery and seam decoding. Support horizontal processing, for example by transposition.
6. Compare greedy and graph cut against DP in terms of **seams, visual quality and running time**.

The stated implementation pipeline concerns removal/shrinking. Enlargement/seam insertion appears in the background; do not let that optional extension displace required removal work. The graph-cut construction must be checked at both image borders and on narrow images. A graph shortest-path solver alone does not satisfy the required max-flow/min-cut alternative.

### Required questions and analysis

- Item 2(a)(i): show that the number of vertical seams grows at least exponentially in image height when width is greater than one.
- Item 2(a)(ii): explain the seam-cost subproblems and how the total cost relates to them.
- Item 2(b): discuss whether divide and conquer improves gradient/convolution computation.
- Explain time and space costs, compare the three approaches, and discuss improvements such as DP storage and multithreading.

Item 2 is a suggested way to get started, not a required source layout. Its questions must still be answered in the report. Provide reasoning and evidence rather than only output screenshots or measured times.

### Required report and presentation

- Report: team ID; all members' names and SIT IDs; Item 2 answers; algorithms, design, implementation, results/evaluation, comparison and discussion; emphasize algorithms and analysis.
- Include an accessible **GitHub code link** and **recorded presentation link** in the report.
- Demonstrate a successfully running app. Each person has **at most 2 minutes**, and the whole team has **at most 12 minutes**.
- Upload the recording to an accessible online platform and check its link.
- Submit the report to **XSite: Assignment 2 Report Only**. Slides, if used, and optional test inputs/outputs go to **Assignment 2 Other Materials**.
- Complete the course's separate administrative paperwork and peer-evaluation process; those records are maintained outside this source scaffold.

The printed deadline is **Friday 20 November, 23:59, Week 12**. In the current course context this is **20 November 2026, Singapore time**. The brief specifies no report page limit or mandatory programming language; check XSite for additional upload constraints.

### Project quiz and optional bonus

The **closed-book, team-based project quiz** is **Tuesday 24 November 2026 during tutorial**, Week 13. It has 10-20 MCQs on the submitted code/report, and may moderate the project score. Team discussion is allowed; external aids are not. Every member needs to understand the implementation and evidence.

Up to **10 bonus marks total** may be awarded for technical complexity and quality of step-by-step visualization of all seams, interactive resizing and CI/CD. No per-feature allocation is given. Implement the required algorithms and evaluation before pursuing bonus work.

## Rubric and evidence to plan for

| Category | Marks | Evidence to develop |
|---|---:|---|
| Design | 20 | Correct, efficient algorithms with clear justification and consideration of alternatives |
| Implementation | 20 | Correct/efficient code, good programming practice and detailed testing across relevant scenarios |
| Analysis | 20 | Accurate, in-depth complexity and arguments supported by results |
| Documentation | 20 | Clear, well-structured report, required components, references and comparative study |
| Presentation | 15 | Professional, accurate explanation, successful demonstration and compliance with time limits |
| Peer evaluation | 5 | Complete the required course evaluation |

Base total: **100 marks**. This checklist helps plan evidence; a build or smoke-test pass does not imply completion or a particular grade.

## Proposed shared conventions - agree before implementing

- `Image`: width/height and a row-major buffer of three 8-bit **RGB** values per pixel. Codec adapters must normalize their library's native channel order.
- `EnergyMap`: matching dimensions and nonnegative signed 64-bit integer costs. If choosing another numeric model, update the interface and all algorithms/tests together.
- `Seam`: one column index per row for a vertical seam; adjacent entries differ by at most one; `cost` is the sum on the supplied current energy map.
- Seam finders consume a fixed map. The resize controller owns image updates and energy recalculation.
- Decide intensity conversion, filter, borders, tie-breaking, alpha/unsupported-format policy and input validation as a team; document choices before comparison.
- Use the same energy, original inputs, target dimensions and axis order for fair comparisons. Compare optimal costs as well as paths; equal-cost seams can have different coordinates.
- Choose a target policy and removal order. A sensible initial scope is positive targets no larger than the original, with vertical-first or horizontal-first explicitly documented.

These conventions are team design proposals, not extra assignment rules. No particular energy filter, codec, graph solver or GUI is selected by the scaffold.

## Layout and remaining work

```text
include/seamcarving/seam.hpp    shared data types and function contracts
src/main.cpp                  working --help / --status launcher
src/image_io.cpp              TODO: load/save adapter
src/energy.cpp                TODO: shared energy function
src/dp.cpp                    TODO: DP and traceback
src/greedy.cpp                TODO: local seam decisions
src/graph_cut.cpp             TODO: actual max-flow/min-cut
src/resize.cpp                TODO: transpose/removal/controller
tests/README.md               tests the team should implement
tests/fixtures/               small energy-map input
data/                         tiny generated RGB fixture
docs/TASKS.md                 work ownership and integration gates
```

There is no completed report, benchmark, GUI, image codec dependency or CI workflow in this starter. Add each when the team implements and verifies it. Use the class notes on DP, greedy reasoning, residual flow networks, recurrence analysis and complexity; unrelated course algorithms do not need to be added.

## Build and check the scaffold

Prerequisites: a C++17 compiler and CMake 3.20+. No Python, OpenCV or extra image library is required to build this scaffold. Windows with Visual Studio C++ desktop tools:

```powershell
cmake -S . -B build
cmake --build build --config Release --parallel 4
ctest --test-dir build -C Release --output-on-failure
.\build\Release\seamcarve.exe --status
```

These direct commands do not need PowerShell script permission. On a single-configuration generator, configure with `-DCMAKE_BUILD_TYPE=Release` and run `./build/seamcarve` (or `build\seamcarve.exe` on Windows Ninja).

Expect **2/2 starter smoke tests passed**, followed by a status listing all implementation modules as **TODO**. These are not the algorithm/CLI/viewer tests of a finished application. Add correctness tests as modules are completed; see [tests/README.md](tests/README.md).

## Suggested first commit

```text
Add seam-carving project scaffold, interfaces and assignment checklist
```

Copy the contents of this starter folder into the repository root, preserving the existing `.git` directory and `.gitattributes`. Keep build output and temporary files out of Git. Agree owners and interfaces together before implementation branches are merged.
