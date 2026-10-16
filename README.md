# ECE555 Neuron Design

Fall 2026 ECE555 design project: design, verify, lay out, and optimize a single MLP neuron with multiply-accumulate and activation logic.

## Current experimental result

**Mirror full adders at 2 NMOS fins / 3 PMOS fins are the leading measured candidate.** The carry-save neuron occupies **6.026400 µm²** (5.022 × 1.200 µm) and has **47.675 ps pre-layout Spectre delay** over all 816 output-changing fixed-weight transitions. A full-history rerun with 1 ps maximum timestep gives 47.647 ps.

The [live design report](https://abhinavnandwani.com/ece555-neuron-design/) includes every beta-sweep point, measured-area rankings, native Virtuoso exports, and the model explorer. The [experiment package](experiments/2026-10-16/README.md) contains 72 sizing screens, seven full timing cases, two numerical reruns, and 14 complete routed layouts with GDS, CDL, extracted MOS connectivity, original PNGs and per-transition measurements.

| Carry-save candidate, N:P fins | Measured boundary area (µm²) | Screen T (ps) | Screen T × A (ps·µm²) | Full T (ps) |
| --- | ---: | ---: | ---: | ---: |
| mirror 2:3 | 6.026400 | 47.174 | 284.291 | 47.675 |
| mirror 3:3 | 6.026400 | 47.388 | 285.577 | 47.736 |
| mirror 2:2 | 5.785344 | 50.706 | 293.351 | 50.891 |
| mirror 4:6 | 6.749568 | 44.924 | 303.218 | Not run |
| aoi-xor 2:2 | 7.840800 | 48.431 | 379.735 | 48.576 |
| aoi-xor 3:3 | 8.154432 | 47.180 | 384.727 | 47.146 |
| cmos-xor 2:2 | 10.987488 | 54.542 | 599.284 | 58.524 |

All 14 layouts have **zero DRC errors across 334 rule checks, LVS CORRECT, and correct extracted MOS connectivity for all 243 legal states**. Their boundaries include routing and physical body taps; testbench input drivers and external load capacitors are excluded. These are regular single-row experimental layouts, not minimum-area implementations.

The common screen uses 103 output-changing transitions. The full history covers 1,944 directed fixed-weight transitions, including 816 output-changing transitions. T × A above combines pre-layout timing with measured physical area. **RC extraction and post-layout timing remain open**, so it is not a final grading result. Mirror 3:3 is only 0.085 ps slower than 2:3 in the 1 ps reruns and has the same measured area. Both remain timing finalists.

Only 14 of the 72 beta-grid points have measured area. The selected layouts cover each topology's fastest and slowest screen point and largest old estimated-area corner, extreme fin ratios, and close sizing comparisons. No physical area is assigned to the other 58 points.

## Project milestones and status

Deadlines come from slide 15 of the [project slide deck](specs/F26_ECE555_project.pptx); deliverables come from the [project specification](specs/F26-Project-ECE555.pdf). All dates are in 2026. Each graded milestone is worth 25 points.

Status last reviewed: **October 16, 2026**. Status reflects evidence committed to this repository; “Not documented” does not establish whether work has started or been submitted outside the repo.

| Milestone | Deadline | Required deliverables / completion criteria | Status | Repository evidence / remaining work |
| --- | --- | --- | --- | --- |
| Team sign-up | October 15 | Register the three-student group. | Unconfirmed | Team registration is not recorded in the repository. |
| MS1: Gate-level neuron design | November 13 | Complete the neuron schematic and submit schematics of the neuron and its submodules, such as the multiplier, adders, and multiplexer. | In progress: preliminary design evidence | [Multiplier Boolean equations](Screenshot%20From%202026-10-16%2012-25-15.png) are present. Complete submodule and integrated neuron schematics are not yet recorded. |
| MS2: Layout and functionality verification | December 4 | Implement the MS1 neuron layout; verify the neuron netlist at **VDD = 0.9 V**, stating inputs, weights, and expected outputs, including the worst-case input combination. Submit submodule and neuron **DRC/LVS** screenshots and neuron **Calibre extraction** screenshots; back up the extracted netlist. | Experimental evidence available | Experimental layouts, timing measurements and DRC/LVS summaries are available in the [experiment package](experiments/2026-10-16/README.md). Student submodule schematics, required screenshots and the milestone submission are not documented. |
| MS3: Optimized layout | December 11 | Identify and optimize the critical path and improve layout/routing; discuss possible improvements. Submit updated submodule and neuron DRC/LVS screenshots and neuron Calibre extraction screenshots, compare parasitic capacitances, and report the final design boundary area. | Experimental evidence available | Fourteen experimental boundary areas and sizing comparisons are measured. Layout compaction, RC parasitic comparisons and final optimized implementation remain open. |
| MS4: Final report and demonstration | December 16 | Test the **extracted neuron netlist at VDD = 0.9 V**, documenting inputs, weights, and expected outputs. Demonstrate correct worst-case output; measure worst-case input-to-output delay **T** and report **f = 1/T**. Prepare the report and demonstration, including design/component explanations, transistor-sizing rationale, and final area. | Not documented | The final report, demonstration evidence, extracted-netlist simulation results, and post-layout timing/frequency measurements are not present. |

Submit each graded milestone to its corresponding Canvas submission area as **one readable PDF per group**, as required by the project specification.

## Design-space model

[`model/neuron.cpp`](model/neuron.cpp) is a transistor-level planning model of the whole neuron. Its estimated areas and modeled delays are separate from the measured experiment results above. Every gate is a static CMOS pull-up/pull-down network with an integer number of fins per device. An event-driven Elmore simulator, including stack-node capacitance, times all 816 transitions that change the output with the weights held fixed. Every design is checked against `ReLU(x1·w1 + x2·w2 + wb)` for all 243 valid input/weight combinations.

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
- **Area**: a historical floorplan estimate with 270 nm cells, four-track routing channels and a 72 nm margin. It assumes 1–3 fins cost no height. The new routed layouts do not follow that assumption: mirror 1:1, 2:2 and 2:3 occupy 5.544288, 5.785344 and 6.026400 µm². The routed 2:3 layout is 2.52 times the old 2.392416 µm² estimate. The model has not been refitted to physical area.

Model results are in [`results/report.txt`](results/report.txt). Use the [measured evidence](experiments/2026-10-16/evidence.json) for the current sizing comparison. The old uniform-2-fin recommendation and estimated-area rankings are superseded by the measured results above. There is no measured spec-datapath layout in this package, so a physical delay-area advantage over that architecture has not been established.

The earlier mirror 2:2 load study measured about 41 ps at 0.2 fF, 51 ps at 1 fF, 99 ps at 5 fF and 159 ps at 10 fF. These sensitivity controls have not been repeated at 2:3; the testbench load must accompany every timing comparison.

### Run it

Requires a C++17 compiler, Python 3.10+ and the GDS reader listed in `tools/requirements.txt`:

```sh
python3 -m venv build/venv
build/venv/bin/python -m pip install -r tools/requirements.txt
make page PYTHON=build/venv/bin/python
```

This checks the saved evidence, independently measures the GDS boundaries, checks all 243 legal states of each extracted transistor network, runs the C++ model, and writes `results/report.txt` and `build/site/index.html`. It does **not** rerun Spectre or Calibre. The generated page is self-contained and can be opened locally. Reproduction with licensed EDA tools is described in the [experiment method](experiments/2026-10-16/README.md#reproduction).

The schematic explorer supports uniform 1:1, 2:2 and 3:3 sizing and stack compensation. It does not display the measured 2:3 sizing. Model parameters can be overridden, for example `./build/neuron --mu 1.2 --cload 20 --stack_beta 1.0`.

### Live page

The [Model and diagram workflow](.github/workflows/model.yml) checks saved evidence, reruns the model and builds the page on pushes and pull requests. On `main` it deploys the [design report](https://abhinavnandwani.com/ece555-neuron-design/) to GitHub Pages. Course milestones require the student's completed design and submission; experiment publication does not establish either.
