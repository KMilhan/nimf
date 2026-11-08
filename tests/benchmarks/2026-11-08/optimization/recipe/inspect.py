from pathlib import Path
import hashlib
import json
import subprocess

controls = [Path('/usr/local/bin/nimf-latency-driver'),
            Path('/usr/local/bin/nimf-latency-gtk3'),
            Path('/usr/local/bin/nimf-latency-gtk4'),
            Path('/usr/bin/mozc_emacs_helper'), Path('/usr/lib/mozc/mozc_server')]
result = {'controls': {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in controls}}
result['flags'] = [line for line in Path('/src/Makefile').read_text().splitlines()
                   if line.startswith(('CFLAGS = ', 'CXXFLAGS = ', 'LDFLAGS = '))]
paths = [Path('/src/bin/nimf/.libs/nimf'), Path('/src/libnimf/.libs/libnimf.so'),
         Path('/src/modules/engines/nimf-mozc/.libs/libnimf-mozc.so')]
paths += sorted(Path('/src/modules/clients/gtk/.libs').glob('*.so'))
result['elf'] = {}
for path in paths:
    sections = subprocess.check_output(['size', '-A', str(path)], text=True)
    text_bytes = int(next(line.split()[1] for line in sections.splitlines() if line.startswith('.text ')))
    result['elf'][str(path.relative_to('/src'))] = {
        'text_bytes': text_bytes, 'file_bytes': path.stat().st_size,
        'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}
result['core_unit_log'] = Path('/src/libnimf/test-memory.log').read_text()
result['mozc_unit_log'] = Path('/src/modules/engines/nimf-mozc/test-nimf-mozc.log').read_text()
print(json.dumps(result, indent=2))
