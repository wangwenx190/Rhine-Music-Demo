"""Compare preserved renderer runs without pooling samples or rewriting old evidence."""

import argparse
import hashlib
import json
from pathlib import Path
import statistics


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def write(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('samples', type=Path)
    parser.add_argument('evidence', type=Path)
    args = parser.parse_args()
    runs = {}
    for variant in ('before', 'after'):
        runs[variant] = []
        for directory in sorted((args.samples / variant).glob('run-*')):
            report = read(directory / 'runtime.json')
            performance = report['performance']
            if not report['windowPolicy']['passed'] or performance['durationSeconds'] != 120:
                raise RuntimeError(f'Invalid sample: {directory}')
            entry = {
                'run': directory.name,
                'performance': performance,
                'reportSha256': sha256(directory / 'runtime.json'),
                'device': report['atlas']['initial']['device'],
                'qt': report['qt'],
                'configuration': report['configuration'],
                'visualCases': len(report['cases']),
                'capacityCases': report.get('capacityCases'),
                'coverSnapshotReuse': report.get('coverSnapshotReuse'),
                'windowPolicyPassed': report['windowPolicy']['passed'],
            }
            measurement = directory / 'measurement.json'
            if measurement.exists():
                entry['process'] = read(measurement)
            runs[variant].append(entry)
            write(args.evidence / variant / (directory.name + '.json'), entry)
        if len(runs[variant]) < 3:
            raise RuntimeError(f'Need three preserved {variant} runs')
    comparisons = []
    for before in runs['before']:
        name = before['run']
        after = next(sample for sample in runs['after'] if sample['run'] == name)
        if before['performance'].get('driver') != after['performance'].get('driver'):
            raise RuntimeError('Different presentation drivers: retain samples but do not compare numeric gains')
        left = args.samples / 'before' / name
        right = args.samples / 'after' / name
        reference = sorted(left.glob('*.png'))
        if not reference:
            raise RuntimeError(f'Missing visual samples: {left}')
        images = []
        for image in reference:
            other = right / image.name
            if not other.exists():
                raise RuntimeError(f'Missing counterpart: {other}')
            images.append({
                'name': image.name,
                'beforeSha256': sha256(image),
                'afterSha256': sha256(other),
                'identical': image.read_bytes() == other.read_bytes(),
            })
        comparisons.append({'run': name, 'images': images})
    medians = {}
    for variant, samples in runs.items():
        medians[variant] = {
            metric: statistics.median(sample['performance'][metric]['p95Ms'] for sample in samples)
            for metric in ('cpuSubmission', 'frameIntervals')
        }
    summary = {
        'scope': 'Release renderer camera-motion throughput; product integration still R10D',
        'runsPerVariant': {key: len(value) for key, value in runs.items()},
        'medianOfRunP95Ms': medians,
        'numericGateResults': {
            key: [sample['performance']['passed'] for sample in value]
            for key, value in runs.items()
        },
        'visualImageCount': sum(len(pair['images']) for pair in comparisons),
        'allImagesByteIdentical': all(image['identical'] for pair in comparisons for image in pair['images']),
        'gpuTimestampAvailable': any(sample['performance']['gpuTimestampAvailable'] for sample in runs['after']),
        'rawSamplesAvailable': {
            key: [bool(sample['performance'].get('frameIntervalSamplesMs')) for sample in value]
            for key, value in runs.items()
        },
        'limitations': [
            'Original binary predates raw-series export; its three full distributions are retained.',
            'First candidate run predates raw-series export; later candidate runs retain chronological values.',
            'No compositor timing, input-to-photon, GPU elapsed or complete-player performance claim.',
            'Formal product evidence uses the complete collection contract at R10D/R28.',
        ],
    }
    write(args.evidence / 'image-comparison.json', comparisons)
    write(args.evidence / 'summary.json', summary)
    if not summary['allImagesByteIdentical']:
        raise RuntimeError('Optimization changed reference images')
    print(json.dumps(summary, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()
