#!/usr/bin/env bash
# Genera web/roms.json con todos los .gba (y .gb/.gbc si los hubiera) que haya
# en web/roms/. El título del menú sale del nombre del archivo.
# Úsalo cada vez que añadas o quites un juego.
set -euo pipefail

cd "$(dirname "$0")/web"
mkdir -p roms

python3 - << 'PY'
import json, os

exts = ('.gba', '.gb', '.gbc')
files = sorted(
    (f for f in os.listdir('roms') if f.lower().endswith(exts)),
    key=str.lower,
)
roms = []
for f in files:
    title = os.path.splitext(f)[0].replace('_', ' ').strip()
    roms.append({'title': title, 'file': 'roms/' + f})

with open('roms.json', 'w', encoding='utf-8') as out:
    json.dump(roms, out, ensure_ascii=False, indent=2)
    out.write('\n')

print('roms.json: %d juego(s)' % len(roms))
for r in roms:
    print('  -', r['title'])
PY
