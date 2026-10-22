"""Compare native detail boxes with independently executed frozen DOM/CSS, no pixels."""
import json
import math
from pathlib import Path
import sys

native, frozen = [json.loads(Path(p).read_text()) for p in sys.argv[1:3]]
IDENTITY_KEYS = ('width', 'height', 'night', 'dpr')
if not native.get('passed') or not native.get('captures') or not frozen.get('captures'):
    raise SystemExit('Passing nonempty native and frozen structure reports required')

def identity(capture):
    return tuple(capture[key] for key in IDENTITY_KEYS)

references = {identity(capture): capture for capture in frozen['captures']}
actual_keys = [identity(capture) for capture in native['captures']]
declared_dprs = {capture['dpr'] for capture in native['captures']}
expected_keys = {key for key in references if key[-1] in declared_dprs}
if (len(references) != len(frozen['captures']) or len(set(actual_keys)) != len(actual_keys)
        or set(actual_keys) != expected_keys):
    raise SystemExit('Exactly one matching native/source record per declared state required')
errors = []
for capture in native['captures']:
    source = references[identity(capture)]
    for name, keys in {
        'article': ['x', 'y', 'width', 'height'], 'caption': ['x', 'y', 'height'],
        'back': ['x', 'y', 'fontSize'],
        'title': ['x', 'y', 'width', 'height', 'fontSize'],
        'artist': ['x', 'y', 'width'], 'facts': ['x', 'y', 'width', 'height', 'gap', 'rowGap'],
        'factLabel': ['x', 'y', 'width', 'height', 'fontSize'],
        'factValue': ['x', 'y', 'width', 'height', 'fontSize'],
        'tabs': ['x', 'y', 'width', 'height'],
        'trackTitle': ['x', 'y', 'width', 'fontSize'],
        'trackArtist': ['x', 'y', 'width', 'fontSize'],
    }.items():
        for key in keys:
            actual, expected = capture['fields'][name][key], source['fields'][name][key]
            if not math.isfinite(actual) or not math.isfinite(expected) or abs(actual - expected) > 1:
                errors.append(dict(width=capture['width'], height=capture['height'],
                                   night=capture['night'], item=name, field=key,
                                   actual=actual, expected=expected))
result = dict(passed=not errors, states=len(native['captures']), toleranceLogicalPx=1, errors=errors)
Path(sys.argv[3]).write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
print(json.dumps(result, ensure_ascii=False)[:12000])
raise SystemExit(0 if result['passed'] else 1)
