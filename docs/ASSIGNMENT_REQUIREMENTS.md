# Assignment requirements and rubric

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
