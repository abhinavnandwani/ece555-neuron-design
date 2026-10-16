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

[`model/neuron.cpp`](model/neuron.cpp) models the whole neuron at transistor level before anything is drawn in Virtuoso. Every gate is a static CMOS pull-up/pull-down network with an integer number of fins per device. An event-driven Elmore simulator, including stack-node capacitance, times all 816 transitions that change the output with the weights held fixed. Every design is checked against `ReLU(x1·w1 + x2·w2 + wb)` for all 243 valid input/weight combinations.

### ASAP7 calibration

The timing model is fitted to 4,896 Spectre transitions ([`calibration/asap7_tt_25c_0p9v.txt`](calibration/asap7_tt_25c_0p9v.txt)). These cover six circuits (spec datapath, carry-save with XOR full adders, carry-save with mirror full adders, each at 1-fin and stack-compensated sizing) in ASAP7 RVT, TT, 25 °C, 0.9 V, L = 20 nm, with 1 fF per output and 1N/2P input drivers.

- The PMOS/NMOS resistance ratio is fixed at the measured drain-current ratio, 49.35 µA / 41.19 µA = 1.20.
- Fitted: τ = 0.59 ps; output and stack-node diffusion of about 1.1 and 1.0 fin gate caps per fin; 1 fF ≈ 20 fin gate caps (about 0.05 fF per fin); and a stack factor that makes a 2-device stack about 1.5× slower, not 2×.
- Worst-case delay of each of the six circuits is within 10% of Spectre, and the per-transition RMS error is 12.7% ([`calibration/fit.txt`](calibration/fit.txt)). The previous planar assumptions (PMOS 2× weaker, no stack-node capacitance) were off by up to 28% and got the sizing conclusions backwards.
- The model ranks transitions well (rank correlation 0.84–0.97 on five circuits) but does not reliably pick the single slowest one. **Take testbench worst-case vectors from Spectre, not the model.**

Rerun the fit after new simulations with `./build/neuron --calibrate calibration/asap7_tt_25c_0p9v.txt`.

### What it explores

- **Architecture**: the spec datapath (multiplier → 2-bit adder → 3-bit adder → ReLU), and a carry-save version that counts nonzero and negative products with two parallel full adders and computes ReLU directly from the counts.
- **Cells**: XOR style (16T NAND, 12T CMOS, 10T NOR+AOI), full adder (XOR-based or mirror), ReLU (3-bit mux or AND gating).
- **Sizing**: 1, 2 or 3 fins everywhere; stack-compensated; and a whole-fin optimizer that upsizes critical-path gates while delay × area improves.
- **Area**: a cell-area estimate for one uniform row of 1–3 fins per device, (2·fins + 4) × 27 nm tall at 54 nm poly pitch, so a 3-fin row matches ASAP7's 270 nm 7.5-track cell. Devices wider than the row fold into extra columns. It excludes routing and whitespace, so it ranks designs but is not the graded boundary area.

Current results are in [`results/report.txt`](results/report.txt). In short:

- Carry-save beats the spec datapath by about 2.3× in delay × area. With Spectre's measured delays and the model's area estimate, the 1-fin carry-save mirror design is best, at 62.1 ps.
- Stack compensation **hurts** the spec datapath in Spectre (79.0 → 93.1 ps). With a 1.2 P/N ratio, PMOS needs little upsizing, and extra fins mostly add load.
- 1-fin rows win on area. Folding a few critical-path gates to 2 fins is the model's best delay × area point; that sizing still needs a Spectre run.

### Run it

```sh
make page    # compile, run all experiments (a few seconds), build the diagram page
```

This writes `results/report.txt` and `build/site/index.html`. The page is self-contained, so open it in any browser. It shows the gate-level schematic of every design with the critical path, the transistor networks and fins of each gate, and the comparison tables. Gate labels and net names on the page match the code.

Parameters can be overridden, for example `./build/neuron --mu 1.2 --cload 20 --stack_beta 1.0`.

### Live page

The [Model and diagram](.github/workflows/model.yml) workflow reruns everything on each push and pull request, and fails if any design produces a wrong output. On `main` it deploys the page to GitHub Pages, so the published page always matches `model/neuron.cpp`.
