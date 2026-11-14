# Milestone 1 gate-level neuron design

The [seven-page report](Group20_ECE555_Milestone1.pdf) documents the complete 112-transistor carry-save neuron and its submodule schematics. Group 20 consists of Arihan Yadav, Abhinav Nandwani, and Jackson Ryan.

## Package contents

| File | Purpose |
| --- | --- |
| [Group20_ECE555_Milestone1.pdf](Group20_ECE555_Milestone1.pdf) | Report exported from Microsoft Word with embedded fonts |
| [Group20_ECE555_Milestone1.docx](Group20_ECE555_Milestone1.docx) | Editable report source |
| [Carry-save datapath.excalidraw](Carry-save%20datapath.excalidraw) | Editable architecture diagram used on page 1 |
| [original/ECE555_Milestone1_GateLevel.pdf](original/ECE555_Milestone1_GateLevel.pdf) | Original Cadence schematic image source |
| [sources/design.py](sources/design.py) | Cell definitions, transistor networks, hierarchy, and switch-level checks |
| [sources/ms1.il](sources/ms1.il) | SKILL script used to construct the Cadence cells and symbols |
| [sources/neuron_netlist.sp](sources/neuron_netlist.sp) | Cadence hspiceD schematic netlist |
| [sources/neuron_tb.sp](sources/neuron_tb.sp) | HSPICE testbench covering all 243 legal input, weight, and bias combinations |
| [sources/sim_result.json](sources/sim_result.json) | Saved October 17, 2026 HSPICE result summary with 243/243 cases passing |
| [sources/fix_report.py](sources/fix_report.py) | Generator for the earlier expanded schematic reference, including connectivity consistency checks |

## Verify the logic

From the repository root, using Python 3, run

```sh
python3 milestones/ms1/sources/design.py
```

The expected final line is `NEURON legal-input check (cases, fails): (243, 0)`. The check evaluates switch-level connectivity. The saved HSPICE summary records transistor-model simulation at 0.9 V, TT, and 25 °C with a 1 fF load per output.

## Edit and export the report

Open `Group20_ECE555_Milestone1.docx` in Microsoft Word. To change the datapath, open `Carry-save datapath.excalidraw` in Excalidraw, edit it, and replace the diagram on page 1 of the Word document. Use Word's PDF export and save as `Group20_ECE555_Milestone1.pdf` in this directory. Check the exported pages and embedded fonts after editing. The current report has seven US Letter pages.

## Rebuild the expanded schematic reference

The earlier Python generator remains available for examining enlarged Cadence panels and checking device connections against `design.py`, `ms1.il`, and the exported netlist. It produces a separate 20-page reference in the ignored `build/ms1/` directory. The current seven-page report is maintained in Word.

The generator requires macOS, Python 3.12, and licensed Arial regular, bold, italic, and bold-italic font files in `/System/Library/Fonts/Supplemental`. Install its dependencies in the ignored build directory and run it from the repository root

```sh
python3 -m venv build/ms1-venv
build/ms1-venv/bin/python -m pip install -r milestones/ms1/requirements.txt
build/ms1-venv/bin/python milestones/ms1/sources/fix_report.py
```

Its default output is `build/ms1/ECE555_MS1_Expanded_Reference.pdf`. The `--output PATH` option selects another destination.

## Cadence and HSPICE environment

Cadence Virtuoso, HSPICE, and the ASAP7 PDK are required to regenerate schematics or rerun the testbench. Before loading `sources/ms1.il`, adapt its library and log directories to your workspace. The script opens cell views in write mode, so use a dedicated `NEURON` library. In `sources/neuron_tb.sp`, update the first `.INCLUDE` to your installed ASAP7 TT model file, then run HSPICE from `sources/` so the relative netlist include resolves.
