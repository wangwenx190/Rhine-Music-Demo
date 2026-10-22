#!/usr/bin/env python3
"""Compare frozen CSS layout anchors, without reading or scoring images."""
import argparse
import json
import math
from pathlib import Path


def compare(native, source):
    if not native.get('passed') or not native.get('browse') or not source.get('captures'):
        raise ValueError('Successful native report and frozen CSS records required')
    records = []
    for sample in native['browse']:
        matches = [entry for entry in source['captures']
                   if all(entry[key] == sample[key] for key in ('width', 'height', 'night'))
                   and abs(entry['dpr'] - sample['dpr']) < .001]
        if len(matches) != 1:
            raise ValueError('Exactly one same-input frozen CSS record required')
        expected = matches[0]
        deltas = {}
        for name in ('header', 'brand', 'browse', 'title', 'play', 'stop', 'footer'):
            properties = ('x', 'y') if name == 'brand' else ('x', 'y', 'width')
            for key in properties:
                delta = abs(sample[name][key] - expected[name][key])
                deltas[f'{name}.{key}'] = delta
        records.append({'width': sample['width'], 'height': sample['height'],
                        'night': sample['night'], 'dpr': sample['dpr'], 'deltas': deltas,
                        'passed': all(math.isfinite(delta) and delta <= 1 for delta in deltas.values())})
    return {'passed': all(x['passed'] for x in records), 'records': records,
            'baseline': source['baseline'], 'sources': source['sources'],
            'scope': 'Logical CSS anchors <=1px; original brand geometry excluded by branding contract'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('native', type=Path)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    result = compare(json.loads(args.native.read_text()), json.loads(args.source.read_text()))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
    print(f"CSS structure: {len(result['records'])} states, passed={result['passed']}")
    return 0 if result['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
