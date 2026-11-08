from collections import defaultdict
import json
from pathlib import Path
import statistics
import sys

ROOT = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path(__file__).resolve().parent
STAGES = ('idle', 'candidate', 'after-100', 'after-1000', 'after-10000', 'closed')
METRICS = ('Rss', 'Pss', 'Pss_Anon', 'Pss_File')
GROUPS = {'stack': {'nimf', 'helper', 'mozc'}, 'nimf': {'nimf'},
          'helper': {'helper'}, 'mozc': {'mozc'}, 'client': {'client'}}

trials = []
for distro in ('ubuntu', 'arch'):
    for variant in ('before', 'after'):
        for number in range(1, 4):
            directory = ROOT / f'{distro}-{variant}-{number}'
            if not (directory / 'PASS').exists():
                continue
            samples = [json.loads(line) for line in
                       (directory / 'samples.jsonl').read_text().splitlines()]
            assert len(samples) == 30, directory
            for stage in STAGES:
                rows = [row for row in samples if row['stage'] == stage]
                assert len(rows) == 5
                for group, roles in GROUPS.items():
                    record = {'distro': distro, 'variant': variant, 'trial': number,
                              'stage': stage, 'group': group, 'mib': {}}
                    for metric in METRICS:
                        values = [sum(process['kib'][metric] for process in row['processes']
                                      if process['role'] in roles) / 1024 for row in rows]
                        record['mib'][metric] = statistics.median(values)
                    trials.append(record)
(ROOT / 'trial-medians.json').write_text(json.dumps(trials, indent=2) + '\n')

grouped = defaultdict(list)
for trial in trials:
    key = tuple(trial[field] for field in ('distro', 'variant', 'stage', 'group'))
    grouped[key].append(trial)
summary = []
for key, rows in grouped.items():
    record = dict(zip(('distro', 'variant', 'stage', 'group'), key))
    record['trials'] = len(rows)
    record['mib'] = {}
    for metric in METRICS:
        values = [row['mib'][metric] for row in rows]
        record['mib'][metric] = {'median': statistics.median(values),
                                 'min': min(values), 'max': max(values)}
    summary.append(record)
(ROOT / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')

lines = ['| OS | Build | Stage | Trials | PSS MiB (range) | RSS MiB (range) | Anonymous PSS MiB |',
         '|---|---|---|---:|---:|---:|---:|']
for row in summary:
    if row['group'] != 'stack':
        continue
    metrics = row['mib']
    values = [f"{metrics[m]['median']:.2f} ({metrics[m]['min']:.2f}–{metrics[m]['max']:.2f})"
              for m in ('Pss', 'Rss')]
    lines.append(f"| {row['distro']} | {row['variant']} | {row['stage']} | {row['trials']} | "
                 + ' | '.join(values) + f" | {metrics['Pss_Anon']['median']:.2f} |")
(ROOT / 'tables.md').write_text('\n'.join(lines) + '\n')
print('\n'.join(lines))
