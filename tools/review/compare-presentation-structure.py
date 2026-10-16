"""Compare native detail boxes with independently executed frozen DOM/CSS, no pixels."""
import json
from pathlib import Path
import sys

native, frozen = [json.loads(Path(p).read_text()) for p in sys.argv[1:3]]
errors = []
for capture in native['captures']:
    source = next(c for c in frozen['captures']
                  if all(c[k] == capture[k] for k in ['width', 'height', 'night', 'dpr']))
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
            if abs(actual - expected) > 1:
                errors.append(dict(width=capture['width'], height=capture['height'],
                                   night=capture['night'], item=name, field=key,
                                   actual=actual, expected=expected))
result = dict(passed=not errors, states=len(native['captures']), toleranceLogicalPx=1, errors=errors)
Path(sys.argv[3]).write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
print(json.dumps(result, ensure_ascii=False)[:12000])
raise SystemExit(0 if result['passed'] else 1)
