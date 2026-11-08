from pathlib import Path
import csv
import json
import os
import subprocess
import sys
import time

ROOT = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path(__file__).resolve().parent
ROOT.mkdir(parents=True, exist_ok=True)

for trial in range(1, 4):
    distros = ('ubuntu', 'arch') if trial != 2 else ('arch', 'ubuntu')
    variants = ('o2', 'o3') if trial != 2 else ('o3', 'o2')
    for distro in distros:
        for variant in variants:
            output = ROOT / f'trial{trial}-{distro}-{variant}'
            output.mkdir()
            image = f'nimf-latency-{distro}:' + ('test' if variant == 'o2' else 'o3')
            (output / 'image.json').write_bytes(subprocess.check_output(
                ['docker', 'image', 'inspect', image]))
            arguments = ['docker', 'create', '--init', '--network', 'none',
                         '--cap-drop', 'ALL', '--security-opt', 'no-new-privileges',
                         '--shm-size', '256m', '-e', 'ITERATIONS=100', '-e', 'WARMUP=10',
                         '-e', 'POLL_US=500', image]
            container = subprocess.check_output(arguments, text=True).strip()
            (output / 'container.txt').write_text(container + '\n')
            metadata = {'image': image, 'trial': trial, 'distro': distro, 'variant': variant,
                        'load_before': os.getloadavg(), 'started': time.time()}
            print('starting ' + output.name, flush=True)
            try:
                with (output / 'runtime.log').open('w') as log:
                    result = subprocess.run(['docker', 'start', '--attach', container],
                                            stdout=log, stderr=log, timeout=180)
            finally:
                subprocess.run(['docker', 'stop', '--timeout', '5', container],
                               stdout=subprocess.DEVNULL, check=True)
                subprocess.run(['docker', 'cp', container + ':/results/.', str(output)], check=True)
                subprocess.run(['docker', 'rm', container], stdout=subprocess.DEVNULL, check=True)
                metadata.update(load_after=os.getloadavg(), ended=time.time())
                (output / 'run.json').write_text(json.dumps(metadata, indent=2) + '\n')
            assert result.returncode == 0, (output, result.returncode)
            log = (output / 'runtime.log').read_text()
            assert log.count('PASS: semantic input without pixels is rejected') == 2
            assert log.count('PASS: visible output from the wrong input context is rejected') == 2
            for toolkit in ('gtk3', 'gtk4'):
                with (output / toolkit / 'samples.csv').open() as stream:
                    rows = list(csv.DictReader(stream))
                assert len(rows) == 333, (output, toolkit)
                for row in rows:
                    stamps = [int(row[key]) for key in
                              ('injected_us', 'key_received_us', 'model_us', 'screen_upper_us')]
                    assert stamps == sorted(stamps), row
                    assert int(row['injected_us']) <= int(row['screen_lower_us']) <= int(row['screen_upper_us'])
                for step in ('preedit', 'commit', 'erase'):
                    assert sum(row['phase'] == 'measured' and row['step'] == step for row in rows) == 100
            (output / 'PASS').write_text('600 measured keys, 4 negative controls, timestamp bounds\n')
            print('passed ' + output.name, flush=True)
