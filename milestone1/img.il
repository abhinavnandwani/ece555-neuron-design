; MS1: export schematic / symbol images (run under Xvfb)
nOut = "/userspace/apyadav/ece555/ms1/img/"
nLog = outfile("/userspace/apyadav/ece555/ms1/img.txt")
procedure(nShot(cell view w h)
  let((win f r)
    win = geOpen(?lib "NEURON" ?cell cell ?view view ?mode "r")
    let((bb bw bh)
      bb = geGetEditCellView(win)~>bBox
      bw = xCoord(cadr(bb)) - xCoord(car(bb)) + 0.8
      bh = yCoord(cadr(bb)) - yCoord(car(bb)) + 0.8
      h = fix(w * bh / bw)
      when(h > 1700 h = 1700 w = fix(h * bw / bh)))
    hiResizeWindow(win list(0:0 w:h))
    let((bb m)
      bb = geGetEditCellView(win)~>bBox
      m = 0.4
      hiZoomIn(win list(list(xCoord(car(bb)) - m yCoord(car(bb)) - m)
                        list(xCoord(cadr(bb)) + m yCoord(cadr(bb)) + m))))
    hiRedraw()
    f = strcat(nOut cell "_" view ".png")
    r = errset(hiExportImage(?fileName f ?window win))
    fprintf(nLog "%s %s -> %L\n" cell view r)
    hiCloseWindow(win)))
errset(
  progn(
    foreach(c list("INV" "NAND2" "NOR2" "NOR3" "XNOR2" "AOI211" "TIELO" "FA_MIRROR")
      nShot(c "schematic" 3000 1300))
    foreach(c list("MULT2" "RELU") nShot(c "schematic" 3000 1100))
    nShot("NEURON" "schematic" 3000 1700)
    foreach(c list("INV" "NAND2" "NOR2" "NOR3" "XNOR2" "AOI211" "TIELO" "FA_MIRROR" "MULT2" "RELU" "NEURON")
      nShot(c "symbol" 900 700)))
  t)
when(errset.errset fprintf(nLog "ERROR %L\n" errset.errset))
close(nLog)
exit()
