"""Reflow the existing MS1 PDF using its original, lossless Cadence images.

Run on macOS with Python, reportlab, numpy, Pillow and pypdf.
No circuit files or original report are changed.
"""
from pathlib import Path
from io import BytesIO
import argparse
import math
import re
import sys
import numpy as np
from PIL import Image
from pypdf import PdfReader, PdfWriter
from reportlab.pdfgen import canvas
from reportlab.lib.utils import ImageReader
from reportlab.lib.styles import ParagraphStyle
from reportlab.platypus import Paragraph, Table, TableStyle
from reportlab.lib import colors
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont

# Embed the fonts so the report renders consistently without system substitution.
for font,filename in [('Helvetica','Arial.ttf'),('Helvetica-Bold','Arial Bold.ttf'),
                      ('Helvetica-Oblique','Arial Italic.ttf'),('Helvetica-BoldOblique','Arial Bold Italic.ttf')]:
    pdfmetrics.registerFont(TTFont(font,str(Path('/System/Library/Fonts/Supplemental')/filename)))
pdfmetrics.registerFontFamily('Helvetica',normal='Helvetica',bold='Helvetica-Bold',
                              italic='Helvetica-Oblique',boldItalic='Helvetica-BoldOblique')

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from design import CELLS, count

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', type=Path, default=HERE.parent / 'ECE555_Milestone1_GateLevel_Ready.pdf')
args = parser.parse_args()
SOURCE = HERE.parent / 'original/ECE555_Milestone1_GateLevel.pdf'
OUTPUT = args.output
OUTPUT.parent.mkdir(parents=True, exist_ok=True)
reader = PdfReader(SOURCE)
images = {im.name: im.image.copy().convert('RGB') for p in reader.pages for im in p.images}
skill = (HERE / 'ms1.il').read_text()
spice = (HERE / 'neuron_netlist.sp').read_text()
W, H, M = 612, 792, 42
INK = colors.HexColor('#18212a')
GRAY = colors.HexColor('#4b5563')
LIGHT = colors.HexColor('#d6dce2')
style = ParagraphStyle('body', fontName='Helvetica', fontSize=10.2, leading=14, textColor=INK)
small = ParagraphStyle('small', parent=style, fontSize=9.4, leading=12.5)

def para(c, text, x, top, width, st=style):
    p=Paragraph(text, st)
    _, h=p.wrap(width, 1000)
    p.drawOn(c,x,top-h)
    return top-h

def fit_image(c, name, box, crop=None):
    """PDF clipping only: preserve the exact original image pixels."""
    im=images[name]
    iw,ih=im.size
    l,t,r,b=crop or (0,0,iw,ih)
    x,y,w,h=box
    s=min(w/(r-l),h/(b-t))
    dx=x+(w-(r-l)*s)/2
    dy=y+(h-(b-t)*s)/2
    c.saveState()
    p=c.beginPath();p.rect(dx,dy,(r-l)*s,(b-t)*s)
    c.clipPath(p,stroke=0,fill=0)
    c.drawImage(ImageReader(im),dx-l*s,dy-(ih-b)*s,iw*s,ih*s)
    c.restoreState()

def runs(v, gap=20):
    q=np.flatnonzero(v); out=[]
    if len(q):
        start=prev=q[0]
        for k in q[1:]:
            if k-prev>gap:out.append((int(start),int(prev)));start=k
            prev=k
        out.append((int(start),int(prev)))
    return out

def tiles(name, cell):
    a=np.asarray(images[name]).astype(int)
    m=(a.max(2)>70)&((a.max(2)-a.min(2))>40)
    cols=runs(m.sum(0)>0)
    cols=[cols[2]] if cell=='INV' else cols[1:-1]
    # Input/output pin banks are represented explicitly in each page header.
    alltiles=[]
    for j,(l,r) in enumerate(cols):
        for i,(t,b) in enumerate(runs(m[:,l:r+1].sum(1)>0)):
            alltiles.append((i,j,(max(0,l-10),max(0,t-10),min(a.shape[1],r+11),min(a.shape[0],b+11))))
    alltiles.sort()
    return [q[2] for q in alltiles]

def positions(cell):
    block=skill.split('; ---- '+cell+'\n',1)[1].split('schCheck(cv)',1)[0]
    rx=r'nMos\(cv "([pn])" "([^"]+)" ([\d.-]+):([\d.-]+) (\d+)\) "([^"]+)" "([^"]+)" "([^"]+)" "([^"]+)"'
    data=[]
    for typ,name,x,y,nfin,d,g,s,b in re.findall(rx,block):
        data.append(dict(name=name,typ=typ,x=float(x),y=float(y),nfin=int(nfin),d=d,g=g,s=s,b=b))
    return sorted(data,key=lambda z:(-z['y'],z['x']))

def verify_sources():
    for cell,obj in CELLS.items():
        if obj['kind']!='leaf':continue
        devs={v[0]:v for v in obj['devs']}
        pos=positions(cell)
        assert len(pos)==len(devs),(cell,len(pos),len(devs))
        for p in pos:
            name,typ,d,g,s,nfin=devs[p['name']]
            assert (typ,d,g,s,nfin)==(p['typ'],p['d'],p['g'],p['s'],p['nfin']), (cell,name)
        block=re.search(r'(?ims)^\.subckt '+re.escape(cell)+r'\s.*?^\.ends '+re.escape(cell)+r'\b',spice).group()
        mos={}
        for line in block.splitlines():
            z=line.split()
            if z and z[0].lower().startswith('m'):
                mos[z[0].upper()]=tuple(s.upper() for s in z[1:5])
        assert len(mos)==len(pos),(cell,'MOS count')
        for p in pos:
            assert mos[p['name']]==tuple(p[k].upper() for k in ('d','g','s','b')),(cell,p['name'],'netlist mismatch')
    # Check every hierarchical pin caption against the Cadence-exported netlist.
    ports={}
    blocks={}
    for mt in re.finditer(r'(?ims)^\.subckt (\w+) ([^\n]+)\n(.*?)^\.ends (\w+)',spice):
        ports[mt[1].upper()]=mt[2].upper().split();blocks[mt[1].upper()]=mt[3]
    # Top level is not wrapped in a .subckt in this exported file.
    blocks['NEURON']=spice.split('** Cell name: NEURON\n',1)[1]
    for cell,obj in CELLS.items():
        if obj['kind']!='hier':continue
        actual={}
        for line in blocks[cell].splitlines():
            z=line.upper().split()
            if z and z[0].startswith('X'):
                actual[z[0][1:]]=(z[-1],dict(zip(ports[z[-1]],z[1:-1])))
        for inst,sub,pins in obj['insts']:
            assert actual[inst][0]==sub
            assert all(actual[inst][1][k.upper()]==v.upper() for k,v in pins.items()),(cell,inst)
    print('Verified: all displayed device/instance connections match design.py, ms1.il, and the Cadence netlist.')

def header(c,title,desc,cell,symbol=None):
    c.setFillColor(INK);c.setFont('Helvetica-Bold',18);c.drawString(M,750,title)
    right=420 if symbol else W-M
    para(c,desc,M,730,right-M)
    obj=CELLS[cell]
    inputs=', '.join(obj['ins']) or 'None'
    outputs=', '.join(obj['outs'])
    para(c,f'<b>Inputs:</b> {inputs}<br/><b>Outputs:</b> {outputs}<br/><b>Supplies:</b> VDD, VSS',M,689,right-M,small)
    if symbol:
        fit_image(c,symbol,(442,650,128,85))
        c.setFont('Helvetica',8);c.drawCentredString(506,640,'Cadence cell symbol')
    c.setStrokeColor(LIGHT);c.line(M,628,W-M,628)

def new_buffer():
    b=BytesIO();return b,canvas.Canvas(b,pagesize=(W,H))

def overlay_intro():
    b,c=new_buffer()
    c.setFillColor(colors.white);c.rect(48,653,520,34,fill=1,stroke=0)
    c.setFillColor(GRAY);c.setFont('Helvetica',10)
    c.drawString(49.5,676,'Single MLP neuron, ASAP7 (7 nm FinFET), Cadence Virtuoso - Fall 2026')
    c.setFont('Helvetica',9.7)
    c.drawString(49.5,660,'Team: Arihan Yadav | Abhinav Nandwani | ____________________')
    # Replace only the clipped architecture drawing, below the original prose.
    c.setFillColor(colors.white);c.rect(46,125,524,168,fill=1,stroke=0)
    c.setFillColor(INK);c.setStrokeColor(INK);c.setLineWidth(.7)
    def box(x,y,w,h,lines):
        c.roundRect(x,y,w,h,4,stroke=1,fill=0)
        for j,text in enumerate(lines):
            c.setFont('Helvetica-Bold' if j==0 else 'Helvetica',8.5 if j==0 else 8)
            c.drawCentredString(x+w/2,y+h-13-j*12,text)
    def wire(points,label=None,xy=None):
        p=c.beginPath();p.moveTo(*points[0])
        for q in points[1:]:p.lineTo(*q)
        c.drawPath(p)
        x,y=points[-1];xp,yp=points[-2];ang=math.atan2(y-yp,x-xp)
        p=c.beginPath();p.moveTo(x,y)
        for da in (2.65,-2.65):p.lineTo(x+4*math.cos(ang+da),y+4*math.sin(ang+da))
        p.close();c.drawPath(p,fill=1,stroke=0)
        if label:
            c.setFont('Helvetica',7.8);c.drawString(*xy,label)
    box(88,235,74,38,['MULT2 (U0)','x0 * w0'])
    box(88,154,74,38,['MULT2 (U1)','x1 * w1'])
    box(242,235,108,38,['FA_MIRROR (U2)','nz = PA0 + PB0 + b0'])
    box(242,154,108,38,['FA_MIRROR (U3)','ng = PA1 + PB1 + b1'])
    box(378,154,47,38,['INV x2','U4, U5'])
    box(453,188,99,59,['RELU (U6)','NOR3 + AOI211','count-to-output'])
    wire([(49,254),(88,254)],'x0, w0',(49,261))
    wire([(49,173),(88,173)],'x1, w1',(49,180))
    wire([(162,260),(242,260)],'PA0',(172,266))
    wire([(162,248),(193,248),(193,180),(242,180)],'PA1',(198,198))
    wire([(162,180),(179,180),(179,242),(242,242)],'PB0',(182,225))
    wire([(162,168),(242,168)],'PB1',(210,157))
    wire([(296,290),(296,273)],'b0',(303,280))
    wire([(296,137),(296,154)],'b1',(303,141))
    wire([(350,254),(444,254),(444,234),(453,234)],'NZ_SN, NZ_CON',(354,263))
    wire([(350,173),(378,173)])
    wire([(425,173),(438,173),(438,204),(453,204)],'NG_S, NG_C',(428,151))
    wire([(502,188),(502,173)],'Z1, Z0',(509,171))
    c.setFont('Helvetica',8.5);c.drawString(386,129,'Z2 = 0, driven by TIELO (U7)')
    c.save();return PdfReader(b).pages[0]

def leaf_pages(c,cell,number,name,symbol,desc):
    ps=positions(cell);ts=tiles(name,cell)
    assert len(ps)==len(ts),(cell,len(ps),len(ts))
    pairs=list(zip(ps,ts))
    chunk=6
    total=math.ceil(len(pairs)/chunk)
    for part in range(total):
        group=pairs[part*chunk:(part+1)*chunk]
        title=f'3.{number} {cell}' + (f' - details {part+1}/{total}' if total>1 else '')
        header(c,title,desc,cell,symbol if part==0 else None)
        para(c,'Enlarged panels from the original Cadence schematic. Equal net labels are electrically connected.',M,614,W-2*M,small)
        if len(group)<=2:cols=1;rows=len(group)
        else:cols=2;rows=math.ceil(len(group)/2)
        cardw=(W-2*M-18*(cols-1))/cols
        cardh=min(260,526/rows)
        for i,(p,crop) in enumerate(group):
            x=M+(i%cols)*(cardw+18);top=579-(i//cols)*cardh
            c.setFont('Helvetica-Bold',11);c.setFillColor(INK)
            c.drawString(x,top,f'{p["name"]}  |  {"PMOS" if p["typ"]=="p" else "NMOS"}  |  {p["nfin"]} fins')
            imageh=cardh-57
            fit_image(c,name,(x,top-15-imageh,cardw,imageh),crop)
            y=top-24-imageh
            c.setFont('Helvetica',10)
            c.drawString(x,y,f'D = {p["d"]}     G = {p["g"]}')
            c.drawString(x,y-13,f'S = {p["s"]}     Bulk = {p["b"]}')
        c.setFont('Helvetica',9);c.setFillColor(GRAY)
        c.drawString(M,37,'ASAP7 RVT: L = 20 nm; NMOS W = 54 nm (2 fins); PMOS W = 81 nm (3 fins).')
        c.showPage()

def hier_pages(c,cell,title,name,symbol,desc):
    insts=CELLS[cell]['insts'];ts=tiles(name,cell)
    assert len(insts)==len(ts),(cell,len(insts),len(ts))
    pages=math.ceil(len(insts)/4)
    for part in range(pages):
        header(c,title+(f' - details {part+1}/{pages}' if pages>1 else ''),desc,cell,symbol if part==0 else None)
        para(c,'Original Cadence instance details with readable pin-to-net labels. Equal net names connect across panels.',M,614,W-2*M,small)
        pairs=list(zip(insts,ts))[part*4:(part+1)*4]
        for i,((inst,sub,pins),crop) in enumerate(pairs):
            x=M+(i%2)*273;top=579-(i//2)*260;cw=255
            c.setFont('Helvetica-Bold',11);c.setFillColor(INK);c.drawString(x,top,f'{inst}  |  {sub}')
            fit_image(c,name,(x,top-174,cw,157),crop)
            text='; '.join(f'{k} = {v}' for k,v in pins.items())
            para(c,text,x,top-181,cw,small)
        c.setFont('Helvetica',9);c.setFillColor(GRAY)
        c.drawString(M,37,'Every instance has VDD = VDD and VSS = VSS; supplies are explicit inputOutput pins.')
        c.showPage()

def final_page(c):
    c.setFont('Helvetica-Bold',20);c.setFillColor(INK);c.drawString(M,750,'4. Neuron summary and verification')
    y=para(c,'The top-level NEURON contains 112 transistors. The preceding pages show all eight top-level instances and every constituent cell.',M,726,W-2*M)
    data=[['Block','Instances','Transistors each','Total'],['MULT2','2','22','44'],['FA_MIRROR','2','24','48'],['INV','2','2','4'],['RELU','1','14','14'],['TIELO','1','2','2'],['NEURON','','','112']]
    tb=Table(data,colWidths=[145,90,155,138],rowHeights=25)
    tb.setStyle(TableStyle([('FONTNAME',(0,0),(-1,0),'Helvetica-Bold'),('FONTNAME',(0,1),(-1,-2),'Helvetica'),('FONTNAME',(0,-1),(-1,-1),'Helvetica-Bold'),('FONTSIZE',(0,0),(-1,-1),10),('BACKGROUND',(0,0),(-1,0),colors.HexColor('#edf0f3')),('BACKGROUND',(0,-1),(-1,-1),colors.HexColor('#edf0f3')),('GRID',(0,0),(-1,-1),.5,LIGHT),('VALIGN',(0,0),(-1,-1),'MIDDLE')]))
    tb.wrapOn(c,528,300);tb.drawOn(c,M,y-195);y-=222
    y=para(c,'<b>Critical path (estimated).</b> x1/w1 to XNOR2 (inverter + AOI22), NOR2, FA_MIRROR (C input), INV, NOR3/AOI211, then Z. The static bias weight drives input A; the late product bits drive C.',M,y,W-2*M)
    y-=24;c.setFont('Helvetica-Bold',16);c.drawString(M,y,'5. Functional verification');y-=17
    y=para(c,'MS1 does not require simulations. The following checks were recorded for the design before layout.',M,y,W-2*M)
    y=para(c,'<b>Switch-level check.</b> Every leaf-gate input and all 243 legal (x0, w0, x1, w1, b) combinations were checked. All 243 produce Z = max(0, y). The saved Cadence connectivity was checked against the source netlists.',M,y-15,W-2*M)
    y=para(c,'<b>Pre-layout HSPICE check.</b> The NEURON schematic was netlisted from Virtuoso (hspiceD) and simulated with ASAP7 TT models at VDD = 0.9 V, 25 degrees C, and 1 fF per output. The testbench applies all 243 legal combinations for 400 ps each and samples Z2 Z1 Z0 at the end of each step. All 243 cases match max(0, y). Sampled logic-1 levels were at least 0.900 V; sampled logic-0 levels were at most 34 microvolts.',M,y-15,W-2*M)
    c.showPage()

def build():
    verify_sources()
    b,c=new_buffer()
    specs=[
      ('INV',1,'X15.png','X17.png','Inverter: Y = NOT A. 2 transistors.'),
      ('NAND2',2,'X18.png','X19.png','Two-input NAND: Y = NOT (A AND B). 4 transistors.'),
      ('NOR2',3,'X22.png','X23.png','Two-input NOR: Y = NOT (A OR B). 4 transistors.'),
      ('XNOR2',4,'X24.png','X25.png','Static CMOS XNOR: Y = A*B + NOT(A)*NOT(B). 12 transistors; input inverters and an AOI22 core.'),
      ('FA_MIRROR',5,'X28.png','X29.png','24-transistor mirror full-adder core. CON = NOT(AB + C(A+B)); SN = NOT(ABC + CON(A+B+C)). Outputs are inverted carry and sum.'),
      ('NOR3',6,'X30.png','X31.png','Three-input NOR: Y = NOT(A OR B OR C). 6 transistors. Produces output Z1 in the ReLU block.'),
      ('AOI211',7,'X34.png','X35.png','AND-OR-INVERT: Y = NOT(A*B + C + D). 8 transistors. Produces output Z0 in the ReLU block.'),
      ('TIELO',8,'X36.png','X37.png','Tie-low cell: Y = 0. 2 transistors. Drives Z2 without connecting the output pin directly to VSS.'),
    ]
    for args in specs:leaf_pages(c,*args)
    hier_pages(c,'MULT2','3.9 MULT2 - two-bit multiplier','X40.png','X41.png','22 transistors. P0 = A0*B0; P1 = A0*B0*(A1 XOR B1). The sign path reuses the NAND output n0.')
    hier_pages(c,'RELU','3.10 RELU - count-to-output logic','X42.png','X43.png','14 transistors. The inverted nonzero-count outputs feed NOR3 and AOI211 directly. The negative count uses true-polarity sum and carry.')
    hier_pages(c,'NEURON','4. Top-level NEURON','X46.png','X47.png','112 transistors. Two multipliers, two mirror full adders, two inverters, the ReLU block, and TIELO compute Z = max(0, x0*w0 + x1*w1 + b).')
    final_page(c);c.save()
    writer=PdfWriter()
    first=reader.pages[0];first.merge_page(overlay_intro());writer.add_page(first)
    writer.add_page(reader.pages[1])
    for page in PdfReader(b).pages:writer.add_page(page)
    total=len(writer.pages)
    for i,page in enumerate(writer.pages):
        fb,fc=new_buffer();fc.setFillColor(GRAY);fc.setFont('Helvetica',8)
        fc.drawRightString(W-M,20,f'{i+1} / {total}');fc.save()
        page.merge_page(PdfReader(fb).pages[0])
    writer.add_metadata({'/Title':'ECE 555 Milestone 1 - Gate-Level Neuron Design','/Author':'Arihan Yadav; Abhinav Nandwani; third team member pending','/Subject':'Readable Cadence schematic report'})
    with OUTPUT.open('wb') as out:writer.write(out)
    final=PdfReader(OUTPUT)
    assert len(final.pages)==total
    text='\n'.join(p.extract_text() for p in final.pages)
    for s in ['Arihan Yadav','Abhinav Nandwani','Z2 = 0, driven by TIELO (U7)']:
        assert s in text,s
    for cell in CELLS:assert cell in text,cell
    print(f'Wrote {OUTPUT} ({total} pages; {OUTPUT.stat().st_size:,} bytes).')

if __name__=='__main__':build()
