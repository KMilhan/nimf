import json
from pathlib import Path
import subprocess
import sys
import time

SCRIPTS = Path(__file__).resolve().parent
ROOT = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else SCRIPTS
ROOT.mkdir(parents=True, exist_ok=True)
RUN_ID = time.time_ns()

for distro in ('ubuntu', 'arch'):
    for trial in range(1, 4):
        # Reverse order in the middle trial to reduce order bias.
        variants = ('before', 'after') if trial != 2 else ('after', 'before')
        for variant in variants:
            name = f'nimf-footprint-{distro}-{variant}-{trial}-{RUN_ID}'
            output = ROOT / f'{distro}-{variant}-{trial}'
            output.mkdir()
            image = (f'nimf-mozc-{distro}:test' if variant == 'before'
                     else f'nimf-footprint-{distro}:after')
            (output / 'image.json').write_bytes(subprocess.check_output(
                ['docker', 'image', 'inspect', image]))
            arguments = [
                'docker', 'run', '--name', name, '--init', '--network', 'none',
                '--cap-drop', 'ALL', '--security-opt', 'no-new-privileges',
                '--shm-size', '256m', '--mount',
                f'type=bind,source={SCRIPTS},target=/probe,readonly',
            ]
            for variable in (
                'XDG_RUNTIME_DIR=/tmp/footprint-runtime',
                'XDG_CONFIG_HOME=/tmp/footprint-config', 'LANG=C.UTF-8',
                'GDK_BACKEND=x11', 'GTK_IM_MODULE=nimf', 'NO_AT_BRIDGE=1',
            ):
                arguments += ['-e', variable]
            arguments += ['--entrypoint', 'bash', image, '/probe/session.sh']
            print('starting ' + name, flush=True)
            with (output / 'runtime.log').open('w') as log:
                try:
                    result = subprocess.run(arguments, stdout=log, stderr=log,
                                            timeout=240)
                finally:
                    subprocess.run(['docker', 'stop', '--timeout', '2', name],
                                   check=True, stdout=subprocess.DEVNULL)
                    subprocess.run(['docker', 'cp', name + ':/results/.', str(output)],
                                   check=True)
            assert result.returncode == 0, (name, result.returncode)
            assert (output / 'PASS').exists(), name
            print('passed ' + name, flush=True)
