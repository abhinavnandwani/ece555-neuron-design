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

The timing model is fitted to 7,764 Spectre transitions in 20 circuits, all ASAP7 RVT, TT, 25 °C, 0.9 V, L = 20 nm, with 1 fF per output and 1N/2P input drivers:

- [`calibration/asap7_tt_25c_0p9v.txt`](calibration/asap7_tt_25c_0p9v.txt): spec datapath, carry-save with XOR full adders, and carry-save with mirror full adders, each at 1-fin and stack sizing (all 816 output-changing transitions each).
- [`calibration/asap7_carry_save_sweep.txt`](calibration/asap7_carry_save_sweep.txt): 14 carry-save topologies and sizings from the carry-save sweep (2-fin everywhere, 2 fins on one block, stack at P/N 1.0 and 1.5).

The PMOS/NMOS resistance ratio is fixed at the measured drain-current ratio, 49.35 µA / 41.19 µA = 1.20. Fitted: τ = 0.53 ps, output and stack-node diffusion of about 1.4 and 1.1 fin gate caps per fin, 1 fF ≈ 13 fin gate caps, and a stack factor that makes a 2-device stack about 1.8× slower than one device.

How well it fits ([`calibration/fit.txt`](calibration/fit.txt)):

- Every circuit's worst-case delay is within 8.5% of Spectre. The model reads slightly low (by about 5% on average), so treat its delays as optimistic.
- It orders 173 of 190 circuit pairs the same way Spectre does.
- Before refitting, the model fitted only to the first file predicted the 14 sweep circuits it had never seen to within 15% (8.8% per-transition RMS), so it extrapolates reasonably.
- It ranks transitions well but does not reliably pick the single slowest one. **Take testbench worst-case vectors from Spectre, not the model.**
- It over-credits upsizing only some gates. Spectre shows partial 2-fin sizing helps much less than uniform 2-fin sizing.

Refit after new simulations with `./build/neuron --calibrate calibration/asap7_tt_25c_0p9v.txt,calibration/asap7_carry_save_sweep.txt`, or check a new data file without refitting using `--validate FILE`.

### What it explores

- **Architecture**: the spec datapath (multiplier → 2-bit adder → 3-bit adder → ReLU), and a carry-save version that counts nonzero and negative products with two parallel full adders and computes ReLU directly from the counts.
- **Cells**: XOR style (16T NAND, 12T CMOS, 10T NOR+AOI), full adder (XOR-based or mirror), ReLU (3-bit mux or AND gating).
- **Sizing**: 1, 2 or 3 fins everywhere; stack-compensated; and a whole-fin optimizer that upsizes critical-path gates while delay × area improves.
- **Area**: the bounding box of an estimated floorplan, using the area study's template (`area-study-2026-10-16/estimate.py`): ASAP7-style cells 270 nm tall, so 1–3 fins per device cost no height, plus 4-track routing channels and a 72 nm margin. It reproduces the study's areas within 3%. It's a planning estimate, not a routed layout.

Current results are in [`results/report.txt`](results/report.txt). Measured in Spectre so far:

| Carry-save, 10T multiplier XNOR, mirror full adders, NOR3/AOI211 output | Fins | Worst delay (816 transitions) |
|---|---|---|
| 1 fin everywhere | 106 | 61.5 ps |
| 2 fins everywhere | 212 | 50.9 ps |
| Previous best (XOR full adders, stack sizing) | 268 | 54.3 ps |

- Carry-save beats the spec datapath by more than 2× in delay × area.
- Stack compensation hurts the spec datapath (79.0 → 93.1 ps). Uniform 2 fins beats every partial or stack sizing tried.
- 2 fins beats 1 fin on delay × area. The area study found that the installed ASAP7 library fits 1, 2 or 3 fins per device in the same 270 nm cell (INVxp33, INVxp67 and INVx1 all measure 0.162 × 0.270 µm). With routing channels included, uniform 2 fins had the lowest delay × area in all 18 layout scenarios the study tried. The model predicts uniform 3 fins at about 50.5 ps for the same area, which still needs a Spectre run.
- Delay depends strongly on the output load: the 2-fin design measures 41 ps at 0.2 fF, 51 ps at 1 fF, 99 ps at 5 fF and 159 ps at 10 fF. The reported T depends on the testbench load.

### Run it

```sh
make page    # compile, run all experiments (a few seconds), build the diagram page
```

This writes `results/report.txt` and `build/site/index.html`. The page is self-contained, so open it in any browser. It shows the gate-level schematic of every design with the critical path, the transistor networks and fins of each gate, and the comparison tables. Gate labels and net names on the page match the code.

Parameters can be overridden, for example `./build/neuron --mu 1.2 --cload 20 --stack_beta 1.0`.

### Live page

The [Model and diagram](.github/workflows/model.yml) workflow reruns everything on each push and pull request, and fails if any design produces a wrong output. On `main` it deploys the page to GitHub Pages, so the published page always matches `model/neuron.cpp`.
