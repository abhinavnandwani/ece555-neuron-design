** Generated for: hspiceD
** Generated on: Oct 17 11:10:15 2026
** Design library name: NEURON
** Design cell name: NEURON
** Design view name: schematic


.TEMP 25
.OPTION
+    ARTIST=2
+    INGOLD=2
+    PARHIER=LOCAL
+    PSF=2
+    HIER_DELIM=0

** Library name: NEURON
** Cell name: NAND2
** View name: schematic
.subckt NAND2 a b vdd vss y
mp1 y b vdd vdd pmos_rvt w=81e-9 l=20e-9 nfin=3
mp0 y a vdd vdd pmos_rvt w=81e-9 l=20e-9 nfin=3
mn1 n1 b vss vss nmos_rvt w=54e-9 l=20e-9 nfin=2
mn0 y a n1 vss nmos_rvt w=54e-9 l=20e-9 nfin=2
.ends NAND2
** End of subcircuit definition.

** Library name: NEURON
** Cell name: INV
** View name: schematic
.subckt INV a vdd vss y
mp0 y a vdd vdd pmos_rvt w=81e-9 l=20e-9 nfin=3
mn0 y a vss vss nmos_rvt w=54e-9 l=20e-9 nfin=2
.ends INV
** End of subcircuit definition.

** Library name: NEURON
** Cell name: XNOR2
** View name: schematic
.subckt XNOR2 a b vdd vss y
mp3 y b p1 vdd pmos_rvt w=81e-9 l=20e-9 nfin=3
mp2 y an p1 vdd pmos_rvt w=81e-9 l=20e-9 nfin=3
mp1 p1 bn vdd vdd pmos_rvt w=81e-9 l=20e-9 nfin=3
mp0 p1 a vdd vdd pmos_rvt w=81e-9 l=20e-9 nfin=3
mp5 bn b vdd vdd pmos_rvt w=81e-9 l=20e-9 nfin=3
mp4 an a vdd vdd pmos_rvt w=81e-9 l=20e-9 nfin=3
mn3 n2 b vss vss nmos_rvt w=54e-9 l=20e-9 nfin=2
mn2 y an n2 vss nmos_rvt w=54e-9 l=20e-9 nfin=2
mn1 n1 bn vss vss nmos_rvt w=54e-9 l=20e-9 nfin=2
mn0 y a n1 vss nmos_rvt w=54e-9 l=20e-9 nfin=2
mn5 bn b vss vss nmos_rvt w=54e-9 l=20e-9 nfin=2
mn4 an a vss vss nmos_rvt w=54e-9 l=20e-9 nfin=2
.ends XNOR2
** End of subcircuit definition.

** Library name: NEURON
** Cell name: NOR2
** View name: schematic
.subckt NOR2 a b vdd vss y
mp1 y b p1 vdd pmos_rvt w=81e-9 l=20e-9 nfin=3
mp0 p1 a vdd vdd pmos_rvt w=81e-9 l=20e-9 nfin=3
mn1 y b vss vss nmos_rvt w=54e-9 l=20e-9 nfin=2
mn0 y a vss vss nmos_rvt w=54e-9 l=20e-9 nfin=2
.ends NOR2
** End of subcircuit definition.

** Library name: NEURON
** Cell name: MULT2
** View name: schematic
.subckt MULT2 a0 a1 b0 b1 p0 p1 vdd vss
xi0 a0 b0 vdd vss n0 NAND2
xi1 n0 vdd vss p0 INV
xi2 a1 b1 vdd vss xn XNOR2
xi3 n0 xn vdd vss p1 NOR2
.ends MULT2
** End of subcircuit definition.

** Library name: NEURON
** Cell name: FA_MIRROR
** View name: schematic
.subckt FA_MIRROR a b c con sn vdd vss
mp11 h5 a vdd vdd pmos_rvt w=81e-9 l=20e-9 nfin=3
mp10 h4 b h5 vdd pmos_rvt w=81e-9 l=20e-9 nfin=3
mp9 sn c h4 vdd pmos_rvt w=81e-9 l=20e-9 nfin=3
mp8 sn con h3 vdd pmos_rvt w=81e-9 l=20e-9 nfin=3
mp7 h3 c vdd vdd pmos_rvt w=81e-9 l=20e-9 nfin=3
mp6 h3 b vdd vdd pmos_rvt w=81e-9 l=20e-9 nfin=3
mp5 h3 a vdd vdd pmos_rvt w=81e-9 l=20e-9 nfin=3
mp4 h2 a vdd vdd pmos_rvt w=81e-9 l=20e-9 nfin=3
mp3 con b h2 vdd pmos_rvt w=81e-9 l=20e-9 nfin=3
mp2 con c h1 vdd pmos_rvt w=81e-9 l=20e-9 nfin=3
mp1 h1 b vdd vdd pmos_rvt w=81e-9 l=20e-9 nfin=3
mp0 h1 a vdd vdd pmos_rvt w=81e-9 l=20e-9 nfin=3
mn11 g5 a vss vss nmos_rvt w=54e-9 l=20e-9 nfin=2
mn10 g4 b g5 vss nmos_rvt w=54e-9 l=20e-9 nfin=2
mn9 sn c g4 vss nmos_rvt w=54e-9 l=20e-9 nfin=2
mn8 sn con g3 vss nmos_rvt w=54e-9 l=20e-9 nfin=2
mn7 g3 c vss vss nmos_rvt w=54e-9 l=20e-9 nfin=2
mn6 g3 b vss vss nmos_rvt w=54e-9 l=20e-9 nfin=2
mn5 g3 a vss vss nmos_rvt w=54e-9 l=20e-9 nfin=2
mn4 g2 a vss vss nmos_rvt w=54e-9 l=20e-9 nfin=2
mn3 con b g2 vss nmos_rvt w=54e-9 l=20e-9 nfin=2
mn2 con c g1 vss nmos_rvt w=54e-9 l=20e-9 nfin=2
mn1 g1 b vss vss nmos_rvt w=54e-9 l=20e-9 nfin=2
mn0 g1 a vss vss nmos_rvt w=54e-9 l=20e-9 nfin=2
.ends FA_MIRROR
** End of subcircuit definition.

** Library name: NEURON
** Cell name: NOR3
** View name: schematic
.subckt NOR3 a b c vdd vss y
mp2 y c p2 vdd pmos_rvt w=81e-9 l=20e-9 nfin=3
mp1 p2 b p1 vdd pmos_rvt w=81e-9 l=20e-9 nfin=3
mp0 p1 a vdd vdd pmos_rvt w=81e-9 l=20e-9 nfin=3
mn2 y c vss vss nmos_rvt w=54e-9 l=20e-9 nfin=2
mn1 y b vss vss nmos_rvt w=54e-9 l=20e-9 nfin=2
mn0 y a vss vss nmos_rvt w=54e-9 l=20e-9 nfin=2
.ends NOR3
** End of subcircuit definition.

** Library name: NEURON
** Cell name: AOI211
** View name: schematic
.subckt AOI211 a b c d vdd vss y
mp3 y b p2 vdd pmos_rvt w=81e-9 l=20e-9 nfin=3
mp2 y a p2 vdd pmos_rvt w=81e-9 l=20e-9 nfin=3
mp1 p2 d p1 vdd pmos_rvt w=81e-9 l=20e-9 nfin=3
mp0 p1 c vdd vdd pmos_rvt w=81e-9 l=20e-9 nfin=3
mn3 y d vss vss nmos_rvt w=54e-9 l=20e-9 nfin=2
mn2 y c vss vss nmos_rvt w=54e-9 l=20e-9 nfin=2
mn1 n1 b vss vss nmos_rvt w=54e-9 l=20e-9 nfin=2
mn0 y a n1 vss nmos_rvt w=54e-9 l=20e-9 nfin=2
.ends AOI211
** End of subcircuit definition.

** Library name: NEURON
** Cell name: RELU
** View name: schematic
.subckt RELU ng_c ng_s nz_con nz_sn vdd vss z0 z1
xi0 nz_con ng_s ng_c vdd vss z1 NOR3
xi1 nz_con ng_s nz_sn ng_c vdd vss z0 AOI211
.ends RELU
** End of subcircuit definition.

** Library name: NEURON
** Cell name: TIELO
** View name: schematic
.subckt TIELO vdd vss y
mp0 h h vdd vdd pmos_rvt w=81e-9 l=20e-9 nfin=3
mn0 y h vss vss nmos_rvt w=54e-9 l=20e-9 nfin=2
.ends TIELO
** End of subcircuit definition.

** Library name: NEURON
** Cell name: NEURON
** View name: schematic
xu1 x1_0 x1_1 w1_0 w1_1 pb0 pb1 vdd vss MULT2
xu0 x0_0 x0_1 w0_0 w0_1 pa0 pa1 vdd vss MULT2
xu3 b_1 pa1 pb1 ng_con ng_sn vdd vss FA_MIRROR
xu2 b_0 pa0 pb0 nz_con nz_sn vdd vss FA_MIRROR
xu5 ng_con vdd vss ng_c INV
xu4 ng_sn vdd vss ng_s INV
xu6 ng_c ng_s nz_con nz_sn vdd vss z0 z1 RELU
xu7 vdd vss z2 TIELO
