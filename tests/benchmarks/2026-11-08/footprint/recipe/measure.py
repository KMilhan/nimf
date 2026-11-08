import json
import os
from pathlib import Path
import select
import subprocess
import time

OUT = Path('/results')
NAMES = {'nimf': 'nimf', 'mozc_emacs_helper': 'helper',
         'mozc_server': 'mozc', 'footprint-probe': 'client'}
FIELDS = ('Rss', 'Pss', 'Pss_Anon', 'Pss_File', 'Private_Clean', 'Private_Dirty')


def settings(schema, key, value):
    subprocess.run(['gsettings', 'set', schema, key, value], check=True)


def sample(stage, index):
    processes = []
    for path in Path('/proc').iterdir():
        if not path.name.isdigit():
            continue
        try:
            executable = (path / 'exe').readlink().name
            if executable not in NAMES:
                continue
            values = {}
            for line in (path / 'smaps_rollup').read_text().splitlines():
                parts = line.split()
                field = parts[0].rstrip(':')
                if field in FIELDS:
                    values[field] = int(parts[1])
            processes.append({'pid': int(path.name), 'name': executable,
                              'role': NAMES[executable], 'kib': values})
        except (FileNotFoundError, PermissionError):
            continue
    expected = {'nimf'} if stage == 'idle' else {'nimf', 'helper', 'mozc'}
    roles = {process['role'] for process in processes}
    if stage not in ('idle', 'closed'):
        expected.add('client')
    assert expected <= roles, (stage, roles)
    assert len(processes) == len(roles), processes
    record = {'stage': stage, 'sample': index, 'processes': processes}
    with (OUT / 'samples.jsonl').open('a') as stream:
        stream.write(json.dumps(record) + '\n')


def snapshot(stage):
    time.sleep(0.5)
    for index in range(5):
        sample(stage, index)
        time.sleep(0.1)
    print('measured ' + stage, flush=True)


def response(expected):
    assert select.select([client.stdout], [], [], 120)[0], 'probe timeout'
    actual = client.stdout.readline().strip()
    assert actual == expected, (actual, expected, client.poll())


def command(value, expected):
    client.stdin.write(value + '\n')
    client.stdin.flush()
    response(expected)


settings('org.nimf', 'setup-environment-variables', 'false')
for name in ('rime', 'anthy', 'libhangul'):
    settings('org.nimf.engines.nimf-' + name, 'active-engine', 'false')
settings('org.nimf.engines.nimf-mozc', 'active-engine', 'true')
settings('org.nimf.engines', 'default-engine', 'nimf-mozc')
settings('org.nimf', 'hidden-active-engines', "['nimf-system-keyboard', 'nimf-mozc']")

metadata = {
    'os': Path('/etc/os-release').read_text(),
    'flags': {line.split(' = ', 1)[0]: line.split(' = ', 1)[1]
              for line in Path('/src/Makefile').read_text().splitlines()
              if line.startswith(('CFLAGS = ', 'CXXFLAGS = ', 'LDFLAGS = '))},
    'versions': subprocess.check_output(
        ['pkg-config', '--modversion', 'glib-2.0', 'gtk+-3.0', 'nimf'], text=True),
    'ldd': subprocess.check_output(['ldd', '/usr/bin/nimf'], text=True),
    'environment': {key: os.environ.get(key) for key in (
        'LANG', 'GDK_BACKEND', 'GTK_IM_MODULE', 'NO_AT_BRIDGE')},
}
assert 'libasan' not in metadata['ldd'] and 'libubsan' not in metadata['ldd']
(OUT / 'metadata.json').write_text(json.dumps(metadata, indent=2) + '\n')

server_log = (OUT / 'nimf.log').open('w')
server = subprocess.Popen(['nimf', '--no-daemon'], stdout=server_log, stderr=server_log)
client = None
try:
    socket = Path(os.environ['XDG_RUNTIME_DIR']) / 'nimf/socket'
    for _ in range(100):
        assert server.poll() is None, 'daemon exited before readiness'
        if socket.exists():
            break
        time.sleep(0.1)
    else:
        raise RuntimeError('daemon socket did not appear')
    time.sleep(4)
    snapshot('idle')
    with (OUT / 'client.log').open('w') as client_log:
        client = subprocess.Popen(['/tmp/footprint-probe'], stdin=subprocess.PIPE,
                                  stdout=subprocess.PIPE, stderr=client_log,
                                  text=True, bufsize=1)
        response('ready')
        command('compose', 'candidate')
        snapshot('candidate')
        for count in (100, 1000, 10000):
            command(f'until {count}', f'commits {count}')
            snapshot(f'after-{count}')
        command('quit', 'closed 10000')
        assert client.wait(timeout=10) == 0
        snapshot('closed')
    server.terminate()
    assert server.wait(timeout=10) == 0
    (OUT / 'PASS').write_text('10000 verified Japanese commits; all stages sampled\n')
finally:
    if client is not None and client.poll() is None:
        client.kill()
        client.wait()
    if server.poll() is None:
        server.kill()
        server.wait()
    server_log.close()
