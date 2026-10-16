** Generated for: hspiceD
** Generated on: Oct 16 13:27:22 2026
** Design library name: ECE555
** Design cell name: XNOR2
** Design view name: schematic
.GLOBAL vdd! vss!


.TEMP 25
.OPTION
+    ARTIST=2
+    INGOLD=2
+    PARHIER=LOCAL
+    PSF=2
+    HIER_DELIM=0

** Library name: ECE555
** Cell name: INV
** View name: schematic
.subckt INV in out
xm3 out in vss! vss! nmos_rvt w=27e-9 l=20e-9 nfin=1
xm2 out in vdd! vdd! pmos_rvt w=54e-9 l=20e-9 nfin=2
.ends INV
** End of subcircuit definition.

** Library name: ECE555
** Cell name: XNOR2
** View name: schematic
xm3 net11 net14 vss! vss! nmos_rvt w=27e-9 l=20e-9 nfin=1
xm2 net26 net10 net11 net11 nmos_rvt w=27e-9 l=20e-9 nfin=1
xm1 net3 b vss! vss! nmos_rvt w=27e-9 l=20e-9 nfin=1
xm0 y a net3 net3 nmos_rvt w=27e-9 l=20e-9 nfin=1
xm7 net26 b net13 net13 pmos_rvt w=27e-9 l=20e-9 nfin=1
xm6 y net14 net4 net4 pmos_rvt w=27e-9 l=20e-9 nfin=1
xm5 net13 net10 vdd! vdd! pmos_rvt w=27e-9 l=20e-9 nfin=1
xm4 net4 a net8 net8 pmos_rvt w=27e-9 l=20e-9 nfin=1
xi3 a net10 INV
xi2 b net14 INV
.END
