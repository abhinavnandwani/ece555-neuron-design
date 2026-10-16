# ECE555 Neuron Design

Fall 2026 ECE555 design project: design, verify, lay out, and optimize a single MLP neuron with multiply-accumulate and activation logic.

## Project milestones and status

Deadlines come from slide 15 of the [project slide deck](specs/F26_ECE555_project.pptx); deliverables come from the [project specification](specs/F26-Project-ECE555.pdf). All dates are in 2026. Each graded milestone is worth 25 points.

Status last reviewed: **October 16, 2026**. Status reflects evidence committed to this repository; “Not documented” does not establish whether work has started or been submitted outside the repo.

| Milestone | Deadline | Required deliverables / completion criteria | Status | Repository evidence / remaining work |
| --- | --- | --- | --- | --- |
| Team sign-up | October 15 | Register the three-student group. | Unconfirmed | Team registration is not recorded in the repository. |
| MS1 — Gate-level neuron design | November 13 | Complete the neuron schematic and submit schematics of the neuron and its submodules, such as the multiplier, adders, and multiplexer. | In progress — preliminary design evidence | [Multiplier Boolean equations](Screenshot%20From%202026-10-16%2012-25-15.png) are present. Complete submodule and integrated neuron schematics are not yet recorded. |
| MS2 — Layout and functionality verification | December 4 | Implement the MS1 neuron layout; verify the neuron netlist at **VDD = 0.9 V**, stating inputs, weights, and expected outputs, including the worst-case input combination. Submit submodule and neuron **DRC/LVS** screenshots and neuron **Calibre extraction** screenshots; back up the extracted netlist. | Not documented | Layouts, testbench results, DRC/LVS evidence, and extraction artifacts are not present. |
| MS3 — Optimized layout | December 11 | Identify and optimize the critical path and improve layout/routing; discuss possible improvements. Submit updated submodule and neuron DRC/LVS screenshots and neuron Calibre extraction screenshots, compare parasitic capacitances, and report the final design boundary area. | Not documented | Optimized layouts, verification/extraction results, capacitance comparisons, and area measurements are not present. |
| MS4 — Final report and demonstration | December 16 | Test the **extracted neuron netlist at VDD = 0.9 V**, documenting inputs, weights, and expected outputs. Demonstrate correct worst-case output; measure worst-case input-to-output delay **T** and report **f = 1/T**. Prepare the report and demonstration, including design/component explanations, transistor-sizing rationale, and final area. | Not documented | The final report, demonstration evidence, extracted-netlist simulation results, and timing/frequency measurements are not present. |

Submit each graded milestone to its corresponding Canvas submission area as **one readable PDF per group**, as required by the project specification.

## Design-space model

[`model/neuron.cpp`](model/neuron.cpp) models the whole neuron at transistor level before anything is drawn in Virtuoso. Every gate is a static CMOS pull-up/pull-down network with real device widths, and an event-driven RC simulator times all 30,244 input transitions that change the output. It checks every design against `ReLU(x1·w1 + x2·w2 + wb)` for all 243 valid input/weight combinations.

It explores:

- **Architecture**: the spec datapath (multiplier → 2-bit adder → 3-bit adder → ReLU), and a carry-save version that counts nonzero and negative products with two parallel full adders and computes ReLU directly from the counts.
- **Cells**: XOR style (16T NAND, 12T CMOS, 10T NOR+AOI), full adder (XOR-based or mirror), ReLU (3-bit mux or AND gating).
- **Sizing**: greedy critical-path upsizing, reported at unit, balanced (min delay × area) and fastest points.
- **Beta**: a global P/N ratio sweep.

Current results are in [`results/report.txt`](results/report.txt). The best carry-save design is about 36% faster and 29% smaller than the best spec datapath. Times are in τ, the RC of one minimum NMOS driving one unit of gate capacitance. The model ignores stack-node capacitance, input slew and wiring, so use it to rank designs and choose test vectors, then confirm in Spectre and on the extracted netlist.

### Run it

```sh
make page    # compile, run all experiments (~30 s), build the diagram page
```

This writes `results/report.txt` and `build/site/index.html`. The page is self-contained, so open it in any browser. It shows the gate-level schematic of every design with the worst-case path, the transistor networks and widths of each gate, and the comparison tables. Gate labels and net names on the page match the code.

Model parameters can be overridden, for example after measuring the PMOS/NMOS strength ratio of the PDK in Spectre:

```sh
./build/neuron --mu 1.8 --beta 1.5 --cload 6
```

### Live page

The [Model and diagram](.github/workflows/model.yml) workflow reruns everything on each push and pull request, and fails if any design produces a wrong output. On `main` it deploys the page to GitHub Pages, so the published page always matches `model/neuron.cpp`.
