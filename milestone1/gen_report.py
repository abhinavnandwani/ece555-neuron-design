"""Build the MS1 report (HTML -> PDF with headless Chromium)."""
import base64, os, subprocess, sys, json
sys.path.insert(0, os.path.dirname(__file__))
from design import CELLS, count
D = os.path.dirname(os.path.abspath(__file__))
IMG = os.path.join(D, 'imgc')
sim = json.load(open(os.path.join(D, 'sim_result.json'))) if os.path.exists(os.path.join(D, 'sim_result.json')) else None

def img(name, cls='sch'):
    p = os.path.join(IMG, name + '.png')
    if not os.path.exists(p): return f'<p class="miss">[missing {name}]</p>'
    return f'<img class="{cls}" src="data:image/png;base64,{base64.b64encode(open(p,"rb").read()).decode()}">'

ENC = {-1: '11', 0: '00', 1: '01'}
def mult_rows():
    r = ''
    for x in (-1, 0, 1):
        for w in (-1, 0, 1):
            r += f'<tr><td>{x:+d}</td><td>{ENC[x]}</td><td>{w:+d}</td><td>{ENC[w]}</td><td>{x*w:+d}</td><td>{ENC[x*w]}</td></tr>'
    return r

def relu_rows():
    r = ''
    for nz in range(4):
        for ng in range(nz + 1):
            y = nz - 2 * ng; z = max(0, y)
            r += f'<tr><td>{nz}</td><td>{nz>>1}{nz&1}</td><td>{ng}</td><td>{ng>>1}{ng&1}</td><td>{y:+d}</td><td>{z}</td><td>0{z>>1}{z&1}</td></tr>'
    return r

SUB = [
 ('INV', 'Inverter', 'Y = A&#772;', 'Used inside the multiplier (P0 = NOT n0) and to restore true-polarity sum and carry for the negative-count adder.'),
 ('NAND2', '2-input NAND', 'Y = (A&middot;B)&#773;', 'Multiplier: n0 = (x&#8320;&middot;w&#8320;)&#773;.'),
 ('NOR2', '2-input NOR', 'Y = (A + B)&#773;', 'Multiplier: P1 = (n0 + xn)&#773; = x&#8320;w&#8320;(x&#8321; &oplus; w&#8321;).'),
 ('XNOR2', '2-input XNOR (static, 12T)', 'Y = A&middot;B + A&#772;&middot;B&#772;', 'AOI22 core on (A, B&#772;) and (A&#772;, B) with two input inverters. Multiplier sign: xn = (x&#8321; &oplus; w&#8321;)&#773;.'),
 ('FA_MIRROR', '28T-style mirror full adder (24T core, inverted outputs)', 'CON = (AB + C(A+B))&#773;, &nbsp; SN = (ABC + CON&middot;(A+B+C))&#773;', 'Counts how many of its three inputs are 1, giving the count in inverted form (CON = &not;carry, SN = &not;sum). C is the input closest to the output node, so the late-arriving product bit goes on C and the static bias bit goes on A.'),
 ('NOR3', '3-input NOR', 'Y = (A + B + C)&#773;', 'ReLU logic: Z1 = (CON<sub>nz</sub> + S<sub>ng</sub> + C<sub>ng</sub>)&#773;.'),
 ('AOI211', 'AND-OR-INVERT 2-1-1', 'Y = (A&middot;B + C + D)&#773;', 'ReLU logic: Z0 = (CON<sub>nz</sub>&middot;S<sub>ng</sub> + SN<sub>nz</sub> + C<sub>ng</sub>)&#773;.'),
 ('TIELO', 'Tie-low cell', 'Y = 0', 'Drives Z2 = 0 (the output can never exceed +3) without shorting the output pin straight to VSS.'),
]

def sub_section(i, cell, title, eq, use):
    n = count(cell)
    return f'''<section class="cell"><h3>3.{i} {cell} &mdash; {title}</h3>
<p><b>Function:</b> {eq} &nbsp;&middot;&nbsp; <b>{n} transistors</b></p><p>{use}</p>
<div class="pair"><div class="s">{img(cell + "_schematic")}</div><div class="y">{img(cell + "_symbol", "sym")}<div class="cap">symbol</div></div></div></section>'''

simtxt = ''
if sim:
    simtxt = f'''<p><b>Pre-layout HSPICE check.</b> The NEURON schematic was netlisted from Virtuoso (hspiceD) and simulated in HSPICE with the ASAP7 TT models at VDD = 0.9 V, 25 &deg;C and 1 fF on each output. The testbench steps through all {sim["cases"]} legal input combinations (400 ps each) and samples Z2 Z1 Z0 at the end of each step. <b>{sim["pass"]} of {sim["cases"]} cases match</b> max(0, y). {sim.get("note","")}</p>'''

html = f'''<!doctype html><html><head><meta charset="utf-8"><title>ECE 555 Milestone 1</title><style>
@page {{ size: Letter; margin: 0.6in 0.6in 0.6in 0.6in; }}
body {{ font-family: "DejaVu Sans", Arial, sans-serif; font-size: 10.5pt; line-height: 1.4; color: #111; }}
h1 {{ font-size: 19pt; margin: 0 0 4px; }} h2 {{ font-size: 13.5pt; border-bottom: 1.5px solid #333; padding-bottom: 2px; margin-top: 18px; }}
h3 {{ font-size: 11.5pt; margin: 10px 0 4px; }}
table {{ border-collapse: collapse; margin: 6px 0; font-size: 9.5pt; }} td, th {{ border: 1px solid #999; padding: 2px 7px; text-align: center; }}
th {{ background: #eee; }} .sub {{ color: #444; }}
img.sch {{ width: 100%; border: 1px solid #ccc; }} img.sym {{ width: 100%; border: 1px solid #ccc; }}
.pair {{ display: flex; gap: 10px; align-items: flex-start; }} .pair .s {{ flex: 3.2; }} .pair .y {{ flex: 1; }}
.cap {{ font-size: 8.5pt; color: #555; text-align: center; }}
section.cell {{ break-inside: avoid; margin-bottom: 8px; }} .pb {{ break-before: page; }}
.tbls {{ display: flex; gap: 22px; align-items: flex-start; break-inside: avoid; }} code {{ font-size: 9.5pt; }}
.miss {{ color: #b00; }}
</style></head><body>
<h1>ECE 555 Project &mdash; Milestone 1: Gate-Level Design</h1>
<div class="sub">Single MLP neuron, ASAP7 (7 nm FinFET), Cadence Virtuoso &nbsp;&middot;&nbsp; Fall 2026 &nbsp;&middot;&nbsp; Team: ______________________</div>

<h2>1. What the neuron computes</h2>
<p>The neuron computes <b>z = ReLU(y)</b>, where <b>y = x&#8320;&middot;w&#8320; + x&#8321;&middot;w&#8321; + b</b>. Inputs, weights and the bias weight are 2-bit two&rsquo;s-complement values restricted to {{&minus;1, 0, +1}}, coded as <code>11</code>, <code>00</code> and <code>01</code>. The code <code>10</code> is never applied. That gives &minus;3 &le; y &le; +3, so z &isin; {{0, 1, 2, 3}} and the output is the 3-bit word Z2 Z1 Z0.</p>

<h2>2. Architecture</h2>
<p>Every term t (the two products and the bias) is itself in {{&minus;1, 0, +1}}, and its two bits have a direct meaning. <b>t&#8320; = 1 means the term is nonzero</b>, and <b>t&#8321; = 1 means the term is negative</b>. If nz is the number of nonzero terms and ng is the number of negative terms, then</p>
<p style="text-align:center"><b>y = (#positive) &minus; (#negative) = nz &minus; 2&middot;ng</b>, &nbsp;&nbsp; z = max(0, nz &minus; 2&middot;ng).</p>
<p>So the datapath is (1) two <b>2-bit multipliers</b>, (2) one <b>full adder counting the three bit-0s</b> (nz) and one <b>full adder counting the three bit-1s</b> (ng), and (3) a small <b>ReLU logic block</b> that turns the two 2-bit counts into z.</p>
<p>This is the spec&rsquo;s multiplier &rarr; 2-bit adder &rarr; 3-bit adder &rarr; 3-bit 2:1 MUX chain in carry-save form. The two full adders add all three operands in one level with no carry ripple between them. The ReLU block merges the sign-selected MUX with the conversion from (nz, ng) to binary. The slides note that the example blocks are optional, and we chose this form because it has the shortest logic depth and the fewest transistors.</p>
<svg viewBox="0 0 760 210" width="100%" style="max-height:210px" font-family="DejaVu Sans, Arial" font-size="12">
<defs><marker id="a" markerWidth="8" markerHeight="8" refX="7" refY="4" orient="auto"><path d="M0,0 L8,4 L0,8 z" fill="#333"/></marker></defs>
<g fill="none" stroke="#333" stroke-width="1.4">
<rect x="70" y="20" width="110" height="60" rx="5"/><rect x="70" y="120" width="110" height="60" rx="5"/>
<rect x="300" y="20" width="140" height="60" rx="5"/><rect x="300" y="120" width="140" height="60" rx="5"/>
<rect x="490" y="120" width="60" height="60" rx="5"/>
<rect x="590" y="40" width="110" height="120" rx="5"/></g>
<g text-anchor="middle"><text x="125" y="45" font-weight="bold">MULT2 (U0)</text><text x="125" y="63">x&#8320; &middot; w&#8320;</text>
<text x="125" y="145" font-weight="bold">MULT2 (U1)</text><text x="125" y="163">x&#8321; &middot; w&#8321;</text>
<text x="370" y="45" font-weight="bold">FA_MIRROR (U2)</text><text x="370" y="63">nz = PA0+PB0+b&#8320;</text>
<text x="370" y="145" font-weight="bold">FA_MIRROR (U3)</text><text x="370" y="163">ng = PA1+PB1+b&#8321;</text>
<text x="520" y="155" font-weight="bold">INV&#215;2</text>
<text x="645" y="95" font-weight="bold">RELU (U6)</text><text x="645" y="113">NOR3 + AOI211</text></g>
<g stroke="#333" stroke-width="1.2" fill="none" marker-end="url(#a)">
<path d="M180,40 H298"/><path d="M180,60 L240,60 L240,140 L298,140"/>
<path d="M180,140 L225,140 L225,52 L298,52"/><path d="M180,160 H298"/>
<path d="M440,50 H588"/><path d="M440,150 H488"/><path d="M550,150 H588"/>
<path d="M700,100 H750"/><path d="M20,50 H68"/><path d="M20,150 H68"/>
<path d="M370,8 V18"/><path d="M370,192 V182"/></g>
<g font-size="11"><text x="22" y="44">x&#8320;, w&#8320;</text><text x="22" y="144">x&#8321;, w&#8321;</text>
<text x="190" y="36">PA0</text><text x="244" y="100">PA1</text><text x="190" y="136">PB0</text><text x="190" y="175">PB1</text>
<text x="378" y="12">b&#8320;</text><text x="378" y="205">b&#8321;</text>
<text x="450" y="45">SN, CON (nz)</text><text x="452" y="144">SN,CON</text><text x="556" y="144">S, C</text>
<text x="704" y="94">Z1 Z0</text><text x="704" y="120">Z2 = 0 (TIELO)</text></g>
</svg>
<h3 class="pb">2.1 Truth tables</h3><div class="tbls"><div><b>Multiplier truth table (legal codes)</b><table><tr><th>x</th><th>x&#8321;x&#8320;</th><th>w</th><th>w&#8321;w&#8320;</th><th>p</th><th>p&#8321;p&#8320;</th></tr>{mult_rows()}</table>
<p>P0 = x&#8320;&middot;w&#8320; &nbsp; P1 = x&#8320;&middot;w&#8320;&middot;(x&#8321; &oplus; w&#8321;)</p></div>
<div><b>ReLU logic (all reachable counts, ng &le; nz)</b><table><tr><th>nz</th><th>c s</th><th>ng</th><th>c s</th><th>y</th><th>z</th><th>Z2Z1Z0</th></tr>{relu_rows()}</table>
<p>Z1 = c<sub>nz</sub>&middot;s&#772;<sub>ng</sub>&middot;c&#772;<sub>ng</sub> &nbsp;&nbsp; Z0 = s<sub>nz</sub>&middot;c&#772;<sub>ng</sub>&middot;(c<sub>nz</sub> + s&#772;<sub>ng</sub>) &nbsp;&nbsp; Z2 = 0</p></div></div>

<h2>3. Sub-module schematics</h2>
<p>Each block is its own cell in library <code>NEURON</code>, with a schematic and a symbol, and higher levels are built only from those symbols. The leaf gates are static CMOS using ASAP7 RVT devices (<code>nmos_rvt</code> / <code>pmos_rvt</code>, L = 20 nm). VDD and VSS are explicit inputOutput pins rather than global nets, as Tutorial 2 recommends for LVS. The initial sizing is <b>2 fins per NMOS and 3 fins per PMOS</b>. Sizing will be refined in Milestones 2 and 3 against the (f<sub>max</sub>/area) metric. Net names are shown as wire labels.</p>
{''.join(sub_section(i+1, *s) for i, s in enumerate(SUB))}
<section class="cell"><h3>3.9 MULT2 &mdash; 2-bit &times; 2-bit multiplier ({count("MULT2")} transistors)</h3>
<p>The multiplier is NAND2 + INV for P0 and XNOR2 + NOR2 for P1. P1 reuses the NAND output n0, so the sign path is XNOR2 &rarr; NOR2, which is two gate levels.</p>
<div class="pair"><div class="s" style="flex:4">{img("MULT2_schematic")}</div><div class="y" style="flex:1">{img("MULT2_symbol","sym")}<div class="cap">symbol</div></div></div></section>
<section class="cell"><h3>3.10 RELU &mdash; count-to-output ReLU logic ({count("RELU")} transistors)</h3>
<p>This block uses the inverted carry and sum of the nonzero counter directly, which saves two inverters, plus the true sum and carry of the negative counter.</p>
<div class="pair"><div class="s" style="flex:4">{img("RELU_schematic")}</div><div class="y" style="flex:1">{img("RELU_symbol","sym")}<div class="cap">symbol</div></div></div></section>

<h2 class="pb">4. Top-level neuron</h2>
{img("NEURON_schematic")}
<div class="pair" style="margin-top:8px"><div class="s"><table>
<tr><th>Block</th><th>Instances</th><th>Transistors each</th><th>Total</th></tr>
<tr><td>MULT2</td><td>2</td><td>{count("MULT2")}</td><td>{2*count("MULT2")}</td></tr>
<tr><td>FA_MIRROR</td><td>2</td><td>{count("FA_MIRROR")}</td><td>{2*count("FA_MIRROR")}</td></tr>
<tr><td>INV</td><td>2</td><td>2</td><td>4</td></tr>
<tr><td>RELU</td><td>1</td><td>{count("RELU")}</td><td>{count("RELU")}</td></tr>
<tr><td>TIELO</td><td>1</td><td>2</td><td>2</td></tr>
<tr><th>NEURON</th><td></td><td></td><th>{count("NEURON")}</th></tr></table></div>
<div class="y" style="flex:0.6">{img("NEURON_symbol","sym")}<div class="cap">NEURON symbol</div></div></div>
<p><b>Critical path (estimated):</b> x&#8321;/w&#8321; &rarr; XNOR2 (inverter + AOI22) &rarr; NOR2 &rarr; FA_MIRROR (C input) &rarr; INV &rarr; NOR3 / AOI211 &rarr; Z. The bias weight is static during operation, so it drives each full adder&rsquo;s slowest input (A), and the late product bits drive C.</p>

<h2>5. Functional verification</h2>
<p>MS1 does not require simulations, but we checked the design two ways before moving to layout. <b>Switch-level check:</b> the same netlists used to generate the Virtuoso cells were evaluated at switch level for every input of every leaf gate, and for all 243 legal (x&#8320;, w&#8320;, x&#8321;, w&#8321;, b) combinations of the full neuron. All 243 produce Z = max(0, y). We also confirmed that the connectivity read back from the saved Virtuoso schematics matches those netlists.</p>
{simtxt}
</body></html>'''
open(os.path.join(D, 'ms1_report.html'), 'w').write(html)
out = os.path.join(D, 'ECE555_Milestone1_GateLevel.pdf')
subprocess.run(['/opt/pw-browsers/chromium-1194/chrome-linux/chrome', '--headless', '--no-sandbox', '--disable-gpu',
                '--no-pdf-header-footer', f'--print-to-pdf={out}', 'file://' + os.path.join(D, 'ms1_report.html')],
               check=True, capture_output=True)
print(out)
