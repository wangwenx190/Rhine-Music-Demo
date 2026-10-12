"""Deterministic shared-theme coordinate search over source-authorized parameters."""
import json, subprocess, hashlib, re
from pathlib import Path
root = Path(__file__).resolve().parents[2]
work = root / 'build/material-calibration'
work.mkdir(parents=True, exist_ok=True)
fixture = root / 'tests/fixtures/materials/single-case.json'
oracle = json.loads(fixture.read_text(encoding='utf-8'))
states = [c for c in oracle['cases'] if c['layer'] == 'all']
keys = [
    'exposureScale',
    'environmentScale',
    'transmissionScale',
    'roughnessScale',
    'thicknessScale',
    'attenuationScale'
]
factors = dict.fromkeys(keys, 1.0)
scans = []
for sweep in range(3):
    changed = False
    for key in keys:
        values = [factors[key] * (1 + d) for d in [0, .1, -.1, .05, -.05, .02, -.02, .01, -.01]]
        if key == 'transmissionScale':
            values = [min(v, 1 / .96) for v in values]
        cases = []
        for value in values:
            candidate = {**factors, key: value}
            for c in states:
                cases.append({**c, 'factors': candidate})
        data = {**oracle, 'calibration': True, 'referenceFixture': str(fixture), 'cases': cases}
        input = work / 'input.json'
        input.write_text(json.dumps(data), encoding='utf-8', newline='\n')
        output = work / 'runtime.json'
        result = subprocess.run(
            [str(root / 'build/release/bin/yalahoo.exe'), '--verify-materials', str(input), '--material-report', str(output)],
            capture_output=True,
            timeout=1200
        )
        if result.returncode or b'Failed to' in result.stderr or b'ERROR:' in result.stderr:
            raise RuntimeError(result.stderr.decode(errors='replace'))
        report = json.loads(output.read_text(encoding='utf-8'))
        if not report['passed']:
            raise RuntimeError('Calibration structural gate failed')
        candidates = []
        for i, value in enumerate(values):
            group = report['cases'][i * len(states): (i + 1) * len(states)]
            # Five source-fixed ROIs are equally weighted across both themes/all views/covers.
            loss = sum(sum(r['meanAbsoluteRgb'] for r in c['roi'].values()) / 5 for c in group) / len(group)
            # Projection term is zero with unchanged source geometry; its measured bound is recorded.
            candidates.append({
                'value': value,
                'loss': loss,
                'maxProjectionPixels': max(c['webProjectionPixels'] for c in group)
            })
        best = min(candidates, key=lambda x: (x['loss'], abs(x['value'] - 1)))
        scans.append({
            'sweep': sweep + 1,
            'parameter': key,
            'inputFactors': dict(factors),
            'candidates': candidates,
            'selected': best
        })
        changed |= abs(best['value'] - factors[key]) > 1e-12
        factors[key] = best['value']
        print('Material scan', sweep + 1, key, 'selected', best, flush=True)
    if not changed:
        break
result = {
    'method': 'shared-theme deterministic coordinate search, current/±10/5/2/1%, maximum three sweeps; IOR/colors/geometry/cover unchanged',
    'order': keys,
    'factors': factors,
    'scans': scans,
    'fixtureSha256': hashlib.sha256(fixture.read_bytes()).hexdigest(),
    'unitCalibration': 'No Qt light intensity units in R07: Three RoomEnvironment/DFG and source BRDF are sampled directly; conversion factor 1. Qt lamp unit scan belongs to R08.'
}
(root / 'verification/windows/materials/calibration.json').write_text(
    json.dumps(result, indent=2) + '\n',
    encoding='utf-8',
    newline='\n'
)

material = root / 'qml/Yalahoo/Music/scene/GlassMaterial.qml'
text = material.read_text(encoding='utf-8')
for key, value in factors.items():
    text = re.sub(
        r'(property real ' + key + r': )[^\n]+',
        lambda m: m[1] + format(value, '.12g'),
        text
    )
material.write_text(text, encoding='utf-8', newline='\n')
