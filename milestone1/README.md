# Milestone 1: gate-level neuron design

Gate-level schematics of the carry-save neuron in Cadence Virtuoso with ASAP7 (RVT, L = 20 nm). This folder covers the MS1 deliverable, one PDF of the submodule and top-level schematics, plus the scripts that built and checked it.

- [`ECE555_Milestone1_GateLevel.pdf`](ECE555_Milestone1_GateLevel.pdf): the 8-page MS1 report. It covers the function, architecture, truth tables, every submodule schematic and symbol, the top-level schematic, transistor count, critical path and verification. The team-member field is blank, so fill it in before submitting.
- [`schematics/`](schematics): PNG exports of each cell's schematic and symbol, straight from Virtuoso.

## Design

Each product and the bias is a term t in {−1, 0, +1}, encoded as t0 = nonzero and t1 = negative. With nz the count of nonzero terms and ng the count of negative ones, y = nz − 2·ng and z = max(0, y).

| Cell | Role | Transistors |
| --- | --- | ---: |
| `MULT2` | 2-bit multiplier: P0 = A0·B0 (NAND + INV), P1 = NOR(NAND(A0,B0), XNOR(A1,B1)) | 2 × 22 |
| `FA_MIRROR` | 24T mirror full adder with inverted outputs (CON, SN). One counts the nonzero bits, one counts the negative bits | 2 × 24 |
| `INV` | Restores the negative-count adder's outputs | 2 × 2 |
| `RELU` | Z1 = NOR3(NZ_CON, NG_S, NG_C); Z0 = AOI211 | 14 |
| `TIELO` | Drives Z2 low (z never exceeds 3) | 2 |
| | **Total** | **112** |

The bias goes on full-adder input A, the slow input. The late product bit goes on C, the fast input. Sizing is 2 NMOS fins and 3 PMOS fins everywhere, the mirror 2:3 point from the [10-16 experiments](../experiments/2026-10-16/README.md). Device width is set to 27 nm per fin (54 nm NMOS, 81 nm PMOS) so it matches what layout extraction reports for LVS.

## Verification

- **Switch level:** `design.py` holds the cell netlists and checks every leaf cell plus the full neuron against ReLU(x0·w0 + x1·w1 + b) for all 243 legal input combinations. All 243 pass.
- **HSPICE, pre-layout:** `sim/neuron_tb.sp` steps through all 243 combinations, 400 ps each, with 1 fF loads at 0.9 V. All 243 pass. Logic 1 settles at 0.900 V or higher and logic 0 at 33 µV or lower ([`sim/sim_result.json`](sim/sim_result.json)). The netlist that ran is [`sim/neuron_netlist.sp`](sim/neuron_netlist.sp).
- **Schematic vs. reference:** `build.txt` is the connectivity dump of the built library. Comparing it with `design.py` finds 0 mismatches across 11 cells.

## Rebuilding

On a CAE machine, with `module load courses/ece555/asap7-cadence-calibre` and `PDK_DIR` set:

```sh
python3 gen_skill.py                                  # writes ms1.il
cadence-run virtuoso -nograph -replay ms1.il          # builds library NEURON (schematics + symbols)
cadence-run virtuoso -nograph -replay net.il          # HSPICE netlist of NEURON
synopsys-run hspice -i sim/neuron_tb.sp -o neuron_tb  # 243-case check
```

`img.il` and `crop.py` export and crop the schematic images, and `gen_report.py` builds the report HTML that becomes the PDF. The scripts use the absolute CAE paths they were run with (`/userspace/apyadav/...`), so change those for another account.
