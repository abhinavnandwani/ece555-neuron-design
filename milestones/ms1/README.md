# Milestone 1: gate-level neuron design

[Open the revised 20-page PDF](ECE555_Milestone1_GateLevel_Ready.pdf).

The November 12, 2026 revision fixes the clipped architecture diagram and replaces small schematic thumbnails with enlarged panels from the original Cadence images. Readable device and instance connection labels accompany those panels. The design remains the 112-transistor carry-save neuron. Arihan Yadav and Abhinav Nandwani are listed on the cover; the third teammate's name is intentionally blank pending confirmation.

**Canvas submission has not been performed.** The assignment page shows a November 13, 2026 deadline at 11:59 PM.

## Package contents

| File | Purpose |
| --- | --- |
| `ECE555_Milestone1_GateLevel_Ready.pdf` | Revised report for team review and eventual submission |
| `original/ECE555_Milestone1_GateLevel.pdf` | Original eight-page report and lossless source of the Cadence images |
| `sources/design.py` | Cell definitions, transistor networks, hierarchy and switch-level checks |
| `sources/ms1.il` | SKILL script used to construct the Cadence cells and symbols |
| `sources/neuron_netlist.sp` | Cadence hspiceD schematic netlist |
| `sources/neuron_tb.sp` | HSPICE testbench covering all 243 legal input/weight/bias combinations |
| `sources/sim_result.json` | Saved October 17, 2026 HSPICE result summary; 243/243 cases passed |
| `sources/fix_report.py` | Rebuilds the revised report from the original PDF and source connectivity |

The report generator checks every device connection against `design.py`, `ms1.il`, and the Cadence-exported netlist, and checks hierarchical instance pin mappings against the exported netlist. Device panels retain the original image pixels and use PDF clipping to remove unused space.

## Verify the logic

From the repository root, using Python 3:

```sh
python3 milestones/ms1/sources/design.py
```

Expected final line: `NEURON legal-input check (cases, fails): (243, 0)`. This is a switch-level connectivity check, not a replacement for transistor-model simulation or timing analysis.

## Rebuild the PDF

The report generator was tested on macOS with Python 3.12. It embeds the Arial regular, bold, italic, and bold-italic fonts from `/System/Library/Fonts/Supplemental`. Those fonts are not distributed in this repository. To run on another platform, adapt the font paths to licensed local font files first.

```sh
python3 -m venv .venv-ms1
.venv-ms1/bin/python -m pip install -r milestones/ms1/requirements.txt
.venv-ms1/bin/python milestones/ms1/sources/fix_report.py
```

The default output replaces the revised PDF in this folder. To preserve that file while checking the build:

```sh
.venv-ms1/bin/python milestones/ms1/sources/fix_report.py --output /tmp/ECE555_MS1_rebuilt.pdf
```

The PDF was visually reviewed on all 20 pages. Rebuilding can change PDF metadata and object ordering; byte-for-byte identity is not promised.

## Cadence and HSPICE environment

The source EDA scripts preserve the original working environment. Before running `ms1.il`, review its library/output directories (`/userspace/apyadav/ece555/...`) and the existing `NEURON` library: the script opens cell views in write mode. In `neuron_tb.sp`, set the first `.INCLUDE` to the installed ASAP7 TT model file. Cadence Virtuoso, HSPICE, and the ASAP7 PDK are required for those steps and are not included here.

The saved HSPICE summary documents the previous run at 0.9 V. Publishing this package does not represent a new HSPICE, DRC/LVS, extraction, or post-layout timing run.
