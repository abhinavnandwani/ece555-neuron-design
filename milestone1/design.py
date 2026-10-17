"""ECE 555 MS1 neuron: single source of truth for the cell netlists.
z = ReLU(x0*w0 + x1*w1 + b), 2-bit codes {-1:11, 0:00, +1:01}.
Architecture (team's carry-save choice): two multipliers -> two mirror full
adders that count nonzero (p[0]) and negative (p[1]) terms -> ReLU logic.
"""
NP, PP = 2, 3   # default fins: NMOS 2, PMOS 3 (N:P = 2:3)

def m(name, typ, d, g, s, nfin=None):
    return (name, typ, d, g, s, nfin or (NP if typ == 'n' else PP))

CELLS = {}
def leaf(name, ins, outs, devs, note=''):
    CELLS[name] = dict(kind='leaf', ins=ins, outs=outs, devs=devs, note=note)
def hier(name, ins, outs, insts, note=''):
    CELLS[name] = dict(kind='hier', ins=ins, outs=outs, insts=insts, note=note)

leaf('INV', ['A'], ['Y'], [m('MP0','p','Y','A','VDD'), m('MN0','n','Y','A','VSS')])
leaf('NAND2', ['A','B'], ['Y'], [
    m('MP0','p','Y','A','VDD'), m('MP1','p','Y','B','VDD'),
    m('MN0','n','Y','A','n1'), m('MN1','n','n1','B','VSS')])
leaf('NOR2', ['A','B'], ['Y'], [
    m('MP0','p','p1','A','VDD'), m('MP1','p','Y','B','p1'),
    m('MN0','n','Y','A','VSS'), m('MN1','n','Y','B','VSS')])
leaf('NOR3', ['A','B','C'], ['Y'], [
    m('MP0','p','p1','A','VDD'), m('MP1','p','p2','B','p1'), m('MP2','p','Y','C','p2'),
    m('MN0','n','Y','A','VSS'), m('MN1','n','Y','B','VSS'), m('MN2','n','Y','C','VSS')])
leaf('XNOR2', ['A','B'], ['Y'], [
    m('MP4','p','AN','A','VDD'), m('MN4','n','AN','A','VSS'),
    m('MP5','p','BN','B','VDD'), m('MN5','n','BN','B','VSS'),
    m('MP0','p','p1','A','VDD'), m('MP1','p','p1','BN','VDD'),
    m('MP2','p','Y','AN','p1'), m('MP3','p','Y','B','p1'),
    m('MN0','n','Y','A','n1'), m('MN1','n','n1','BN','VSS'),
    m('MN2','n','Y','AN','n2'), m('MN3','n','n2','B','VSS')],
    'Y = A*B + A\'*B\' as AOI22 on (A,B\') and (A\',B)')
leaf('AOI211', ['A','B','C','D'], ['Y'], [
    m('MP0','p','p1','C','VDD'), m('MP1','p','p2','D','p1'),
    m('MP2','p','Y','A','p2'), m('MP3','p','Y','B','p2'),
    m('MN0','n','Y','A','n1'), m('MN1','n','n1','B','VSS'),
    m('MN2','n','Y','C','VSS'), m('MN3','n','Y','D','VSS')], "Y = (A*B + C + D)'")
leaf('TIELO', [], ['Y'], [m('MP0','p','h','h','VDD'), m('MN0','n','Y','h','VSS')])
leaf('FA_MIRROR', ['A','B','C'], ['CON','SN'], [
    # carry: CON = (AB + C(A+B))'
    m('MN0','n','g1','A','VSS'), m('MN1','n','g1','B','VSS'), m('MN2','n','CON','C','g1'),
    m('MN3','n','CON','B','g2'), m('MN4','n','g2','A','VSS'),
    m('MP0','p','h1','A','VDD'), m('MP1','p','h1','B','VDD'), m('MP2','p','CON','C','h1'),
    m('MP3','p','CON','B','h2'), m('MP4','p','h2','A','VDD'),
    # sum: SN = (ABC + CON(A+B+C))'
    m('MN5','n','g3','A','VSS'), m('MN6','n','g3','B','VSS'), m('MN7','n','g3','C','VSS'),
    m('MN8','n','SN','CON','g3'),
    m('MN9','n','SN','C','g4'), m('MN10','n','g4','B','g5'), m('MN11','n','g5','A','VSS'),
    m('MP5','p','h3','A','VDD'), m('MP6','p','h3','B','VDD'), m('MP7','p','h3','C','VDD'),
    m('MP8','p','SN','CON','h3'),
    m('MP9','p','SN','C','h4'), m('MP10','p','h4','B','h5'), m('MP11','p','h5','A','VDD')],
    '24T mirror adder, inverted outputs; C is the fast input (next to the output)')

hier('MULT2', ['A1','A0','B1','B0'], ['P1','P0'], [
    ('I0','NAND2', dict(A='A0', B='B0', Y='n0')),
    ('I1','INV',   dict(A='n0', Y='P0')),
    ('I2','XNOR2', dict(A='A1', B='B1', Y='xn')),
    ('I3','NOR2',  dict(A='n0', B='xn', Y='P1'))],
    'P0 = A0*B0, P1 = A0*B0*(A1 xor B1)')
hier('RELU', ['NZ_CON','NZ_SN','NG_S','NG_C'], ['Z1','Z0'], [
    ('I0','NOR3',   dict(A='NZ_CON', B='NG_S', C='NG_C', Y='Z1')),
    ('I1','AOI211', dict(A='NZ_CON', B='NG_S', C='NZ_SN', D='NG_C', Y='Z0'))],
    'z = max(0, nz - 2*ng) from the two counts')
hier('NEURON', ['X0_1','X0_0','W0_1','W0_0','X1_1','X1_0','W1_1','W1_0','B_1','B_0'],
     ['Z2','Z1','Z0'], [
    ('U0','MULT2', dict(A1='X0_1', A0='X0_0', B1='W0_1', B0='W0_0', P1='PA1', P0='PA0')),
    ('U1','MULT2', dict(A1='X1_1', A0='X1_0', B1='W1_1', B0='W1_0', P1='PB1', P0='PB0')),
    ('U2','FA_MIRROR', dict(A='B_0', B='PA0', C='PB0', CON='NZ_CON', SN='NZ_SN')),
    ('U3','FA_MIRROR', dict(A='B_1', B='PA1', C='PB1', CON='NG_CON', SN='NG_SN')),
    ('U4','INV', dict(A='NG_SN', Y='NG_S')),
    ('U5','INV', dict(A='NG_CON', Y='NG_C')),
    ('U6','RELU', dict(NZ_CON='NZ_CON', NZ_SN='NZ_SN', NG_S='NG_S', NG_C='NG_C', Z1='Z1', Z0='Z0')),
    ('U7','TIELO', dict(Y='Z2'))])

ORDER = ['INV','NAND2','NOR2','NOR3','XNOR2','AOI211','TIELO','FA_MIRROR','MULT2','RELU','NEURON']

def flatten(cell, prefix='', conn=None):
    """Return flat list of (type, d, g, s) with global net names."""
    c = CELLS[cell]; conn = conn or {}
    def net(n):
        if n in ('VDD', 'VSS'): return n
        return conn.get(n, prefix + n)
    out = []
    if c['kind'] == 'leaf':
        for (_, t, d, g, s, _f) in c['devs']:
            out.append((t, net(d), net(g), net(s)))
    else:
        for (iname, sub, pins) in c['insts']:
            out += flatten(sub, prefix + iname + '/', {k: net(v) for k, v in pins.items()})
    return out

def count(cell):
    return len(flatten(cell))

def switch_sim(devs, inputs, init=None):
    """Switch-level evaluation of a static CMOS netlist. Internal nodes start
    at init; callers run init=0 and init=1 and require identical outputs, so a
    floating node cannot hide behind a lucky starting value."""
    val = {}
    if init is not None:
        for (_, d, _, s) in devs: val[d] = init; val[s] = init
    val.update(inputs); val['VDD'] = 1; val['VSS'] = 0
    for _ in range(60):
        changed = False
        nodes = {d for (_, d, _, s) in devs} | {s for (_, d, _, s) in devs}
        for nd in nodes - set(val.keys()) | {n for n in nodes if n not in inputs and n not in ('VDD','VSS')}:
            def reach(target, typ):
                seen, st = {nd}, [nd]
                while st:
                    x = st.pop()
                    if x == target: return True
                    if x in ('VDD','VSS'): continue
                    for (t, d, g, s) in devs:
                        if t != typ: continue
                        gv = val.get(g)
                        on = (gv == 1) if t == 'n' else (gv == 0)
                        if not on: continue
                        for a, b in ((d, s), (s, d)):
                            if a == x and b not in seen:
                                seen.add(b); st.append(b)
                return False
            up, dn = reach('VDD', 'p'), reach('VSS', 'n')
            nv = 1 if up and not dn else 0 if dn and not up else ('X' if up and dn else val.get(nd))
            if val.get(nd) != nv:
                val[nd] = nv; changed = True
        if not changed: break
    return val

ENC = {-1: (1, 1), 0: (0, 0), 1: (0, 1)}

def check_neuron():
    devs = flatten('NEURON'); bad = 0; n = 0
    for x0 in (-1, 0, 1):
     for w0 in (-1, 0, 1):
      for x1 in (-1, 0, 1):
       for w1 in (-1, 0, 1):
        for b in (-1, 0, 1):
            ins = {}
            for nm, v in (('X0', x0), ('W0', w0), ('X1', x1), ('W1', w1), ('B', b)):
                ins[nm + '_1'], ins[nm + '_0'] = ENC[v]
            v = switch_sim(devs, ins, 0); v1 = switch_sim(devs, ins, 1)
            if any(v[k] != v1[k] for k in ('Z2','Z1','Z0')): v = {'Z2':'X','Z1':'X','Z0':'X'}
            z = 4 * v['Z2'] + 2 * v['Z1'] + v['Z0'] if all(v[k] in (0, 1) for k in ('Z2','Z1','Z0')) else None
            exp = max(0, x0 * w0 + x1 * w1 + b); n += 1
            if z != exp:
                bad += 1; print('FAIL', x0, w0, x1, w1, b, z, exp)
    return n, bad

def check_cell(cell, fn):
    c = CELLS[cell]; devs = flatten(cell); bad = 0
    import itertools
    for bits in itertools.product((0, 1), repeat=len(c['ins'])):
        ins = dict(zip(c['ins'], bits))
        v0 = switch_sim(devs, ins, 0); v1 = switch_sim(devs, ins, 1)
        got = tuple(v0[o] if v0[o] == v1[o] else 'X' for o in c['outs']); exp = fn(**ins)
        if got != exp: bad += 1; print('FAIL', cell, ins, got, exp)
    return bad

if __name__ == '__main__':
    nb = lambda x: 1 - x
    tests = {
        'INV': lambda A: (nb(A),), 'NAND2': lambda A, B: (nb(A & B),),
        'NOR2': lambda A, B: (nb(A | B),), 'NOR3': lambda A, B, C: (nb(A | B | C),),
        'XNOR2': lambda A, B: (nb(A ^ B),), 'AOI211': lambda A, B, C, D: (nb((A & B) | C | D),),
        'TIELO': lambda: (0,),
        'FA_MIRROR': lambda A, B, C: (nb(int(A + B + C >= 2)), nb((A + B + C) & 1)),
    }
    for k, f in tests.items():
        print(k, 'transistors', count(k), 'fails', check_cell(k, f))
    for k in ('MULT2', 'RELU', 'NEURON'):
        print(k, 'transistors', count(k))
    print('NEURON legal-input check (cases, fails):', check_neuron())
