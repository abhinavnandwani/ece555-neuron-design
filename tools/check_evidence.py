#!/usr/bin/env python3
"""Check saved evidence, GDS boundaries and extracted transistor connectivity.

This does not invoke Spectre or rerun Calibre DRC/LVS.
"""
import hashlib
import itertools
import json
import math
from pathlib import Path
import re

import gdstk

ROOT = Path(__file__).resolve().parents[1] / 'experiments/2026-10-16'
evidence = json.loads((ROOT / 'evidence.json').read_text())
manifest = json.loads((ROOT / 'manifest.json').read_text())
for name, item in manifest['files'].items():
    assert hashlib.sha256((ROOT / name).read_bytes()).hexdigest() == item['sha256'], name

runs = evidence['runs']
screens = {r['name']: r for r in runs if r['stage'] == 'screen'}
assert len(screens) == 72
assert {(r['topology'], r['nfins'], r['pfins']) for r in screens.values()} == set(
    itertools.product(('mirror', 'aoi_xor', 'cmos_xor'), range(1, 5), range(1, 7)))
assert sum(r['stage'] == 'full' for r in runs) == 7
assert sum(r['stage'] == 'numerical' for r in runs) == 2
schedules = {k: json.loads((ROOT / f'spectre/{k}.json').read_text()) for k in ('screen', 'full')}
assert len(schedules['screen']['tests']) == 103
full_pairs = {(tuple(t['a']), tuple(t['b'])) for t in schedules['full']['tests']}
states = list(itertools.product((0, 1, -1), repeat=5))
assert full_pairs == {(a, b) for a in states for b in states if a[2:] == b[2:] and a[:2] != b[:2]}

def expected(v):
    return max(0, v[0] * v[2] + v[1] * v[3] + v[4])

for r in runs:
    name = r['name']
    m = json.loads((ROOT / f'measurements/{name}.json').read_text())
    assert all(r[k] == v for k, v in m['summary'].items()), name
    assert not (m['failures'] or m.get('rail_margin_failure_steps', []) or m['missed']), name
    assert all(r[k] == 0 for k in ('functional_failures', 'rail_margin_failures', 'missed_transitions')), name
    tests = schedules['screen' if r['stage'] == 'screen' else 'full']['tests']
    changing = [t for t in tests if expected(t['a']) != expected(t['b'])]
    assert len(m['transitions']) == len(changing) == r['output_changing_transitions'], name
    for t, ref in zip(m['transitions'], changing):
        assert all(t[k] == ref[k] for k in ('step', 'a', 'b')), name
        assert t['expected_from'] == expected(t['a']) and t['expected_to'] == expected(t['b']), name
    assert r['delay_ps'] == max(t['delay_ps'] for t in m['transitions']), name
    if r['stage'] == 'numerical':
        assert r['spectre_warnings'] == 0 and r['maxstep_ps'] == 1, name


def check_logic(devices, outputs):
    """Solve conducting NMOS/PMOS networks for every legal static input."""
    for vec in states:
        bits = [({0: 0, 1: 1, -1: 3}[x] >> b) & 1 for x in vec for b in (0, 1)]
        pins = dict(zip((f'i{k}' for k in range(10)), bits))
        vals = dict(pins, VDD=1, VSS=0)
        for _ in range(100):
            graph = {}
            for d in devices:
                drain, gate, source = d[1:4]
                if gate in vals and vals[gate] == (d[5] == 'nmos_rvt'):
                    graph.setdefault(drain, set()).add(source)
                    graph.setdefault(source, set()).add(drain)
            new = dict(pins, VDD=1, VSS=0)
            for root, value in [('VDD', 1), ('VSS', 0)]:
                todo, seen = [root], set()
                while todo:
                    node = todo.pop()
                    if node in seen:
                        continue
                    seen.add(node)
                    assert node not in new or new[node] == value, (vec, 'rail short', node)
                    new[node] = value
                    todo.extend(graph.get(node, ()))
            if new == vals:
                break
            vals = new
        else:
            raise AssertionError((vec, 'did not settle'))
        assert all(n in vals for n in outputs), (vec, 'undriven output')
        assert sum(vals[n] << k for k, n in enumerate(outputs)) == expected(vec), vec

layouts = {r['sweep_case']: r for r in evidence['layouts']}
assert len(layouts) == 14
for r in layouts.values():
    name = r['name']
    cell = next(c for c in gdstk.read_gds(ROOT / f'layouts/{name}.gds').cells if c.name == name)
    boundaries = [p for p in cell.polygons if (p.layer, p.datatype) == (100, 0)]
    assert len(boundaries) == 1, name
    lo, hi = boundaries[0].bounding_box()
    width, height = hi[0] - lo[0], hi[1] - lo[1]
    assert all(math.isclose(a, b, abs_tol=1e-9) for a, b in zip(
        (width, height, width * height), (r['width_um'], r['height_um'], r['area_um2']))), name
    native = (ROOT / 'layouts/virtuoso-boundaries.txt').read_text()
    match = re.search(re.escape(name) + r' boundary=.* width=([\d.]+) height=([\d.]+) area=([\d.]+)', native)
    assert match and all(math.isclose(a, float(b), abs_tol=1e-9) for a, b in zip(
        (width, height, width * height), match.groups())), name
    assert r['drc_rule_checks'] == 334 and r['drc_errors'] == 0 and r['lvs_correct'], name
    assert r['physical_body_taps'] and not r['virtual_supply_connections'], name
    assert not r['post_layout_timing_available'], name
    assert r['screen_delay_ps'] == screens[r['sweep_case']]['delay_ps'], name
    assert math.isclose(r['screen_TA_ps_um2'], width * height * r['screen_delay_ps']), name
    ext = (ROOT / f'layouts/{name}.extracted.sp').read_text()
    devices = [line.split() for line in ext.splitlines() if re.match(r'^M\d+ ', line)]
    assert len(devices) == r['transistors'], name
    for model, fins, body in [('nmos_rvt', r['nfins'], 'VSS'), ('pmos_rvt', r['pfins'], 'VDD')]:
        rows = [d for d in devices if d[5] == model]
        assert len(rows) == r['transistors'] // 2, name
        assert all(f'nfin={fins}' in d and 'L=2e-08' in d and d[4] == body for d in rows), name
    cdl = (ROOT / f'layouts/{name}.cdl').read_text()
    ports = re.search(r'^\.SUBCKT \S+ (.*)', cdl, re.M)[1].split()
    check_logic(devices, ports[10:12])

for topology, selected in evidence['coverage'].items():
    group = [r for r in screens.values() if r['topology'] == topology]
    assert min(group, key=lambda r: r['delay_ps'])['name'] == selected['fastest_screen']
    assert max(group, key=lambda r: r['delay_ps'])['name'] == selected['slowest_screen']
    assert all(name in layouts for name in selected.values())
assert sum(r['layout_case'] is None for r in screens.values()) == 58
print('Evidence checks passed: 72 screens, 7 full runs, 2 numerical reruns; 14 GDS boundaries and 14 × 243 extracted static truth cases.')
print('Saved DRC/LVS results verified for consistency; licensed tools were not rerun.')
