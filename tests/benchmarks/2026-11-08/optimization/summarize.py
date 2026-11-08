from collections import defaultdict
import csv
import json
import math
from pathlib import Path
import statistics
import sys

ROOT = Path(__file__).resolve().parent
SOURCE = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else ROOT / 'accepted'
OUTPUT = SOURCE if len(sys.argv) > 1 else ROOT
FIELDS = ('key_ms', 'model_ms', 'processing_ms', 'screen_lower_ms',
          'screen_upper_ms', 'interval_ms', 'readback_ms')
trials = []
first = []
for marker in sorted(SOURCE.glob('trial*/PASS')):
    directory = marker.parent
    metadata = json.loads((directory / 'run.json').read_text())
    for toolkit in ('gtk3', 'gtk4'):
        with (directory / toolkit / 'samples.csv').open() as stream:
            data = list(csv.DictReader(stream))
        for row in data:
            row['processing_ms'] = (int(row['model_us']) - int(row['key_received_us'])) / 1000
        context = {key: metadata[key] for key in ('distro', 'variant', 'trial')}
        context['toolkit'] = toolkit
        initial = next(row for row in data if row['phase'] == 'first' and row['step'] == 'preedit')
        first.append({**context, **{field: float(initial[field]) for field in FIELDS}})
        for step in ('preedit', 'commit', 'erase'):
            selected = [row for row in data if row['phase'] == 'measured' and row['step'] == step]
            assert len(selected) == 100
            metrics = {}
            for field in FIELDS:
                values = sorted(float(row[field]) for row in selected)
                metrics[field] = {f'p{p}': values[math.ceil(len(values)*p/100)-1]
                                  for p in (50, 95, 99, 100)}
            trials.append({**context, 'step': step, 'metrics': metrics})
(OUTPUT / 'trial-stats.json').write_text(json.dumps(trials, indent=2) + '\n')
(OUTPUT / 'first-input.json').write_text(json.dumps(first, indent=2) + '\n')

groups = defaultdict(list)
for row in trials:
    groups[tuple(row[key] for key in ('distro', 'toolkit', 'step', 'variant'))].append(row)
summaries = []
for key, values in groups.items():
    record = dict(zip(('distro', 'toolkit', 'step', 'variant'), key))
    record['trials'] = len(values)
    record['metrics'] = {}
    for field in FIELDS:
        record['metrics'][field] = {}
        for percentile in ('p50', 'p95', 'p99', 'p100'):
            selected = [row['metrics'][field][percentile] for row in values]
            record['metrics'][field][percentile] = {
                'median': statistics.median(selected), 'min': min(selected), 'max': max(selected)}
    summaries.append(record)
(OUTPUT / 'summary.json').write_text(json.dumps(summaries, indent=2) + '\n')

def value(distro, toolkit, step, variant, metric, percentile='p50'):
    row = next((row for row in summaries if
                (row['distro'], row['toolkit'], row['step'], row['variant']) ==
                (distro, toolkit, step, variant)), None)
    return row['metrics'][metric][percentile] if row else None

lines = ['| OS | Toolkit | Step | Metric | O2 median (range) | O3 median (range) | Delta ms |',
         '|---|---|---|---|---:|---:|---:|']
for distro in ('ubuntu', 'arch'):
    for toolkit in ('gtk3', 'gtk4'):
        for step in ('preedit', 'commit', 'erase'):
            for metric, percentile in (('screen_upper_ms','p50'), ('screen_upper_ms','p95'),
                                        ('processing_ms','p50')):
                a = value(distro, toolkit, step, 'o2', metric, percentile)
                b = value(distro, toolkit, step, 'o3', metric, percentile)
                if a is None or b is None:
                    continue
                form = lambda x: f"{x['median']:.3f} ({x['min']:.3f}–{x['max']:.3f})"
                lines.append(f'| {distro} | {toolkit} | {step} | {metric} {percentile} | '
                             f"{form(a)} | {form(b)} | {b['median']-a['median']:+.3f} |")
(OUTPUT / 'tables.md').write_text('\n'.join(lines) + '\n')
print('Completed runs:', len(list(SOURCE.glob('trial*/PASS'))))
for line in lines:
    if '| preedit |' in line or line.startswith('| OS') or line.startswith('|---'):
        print(line)
