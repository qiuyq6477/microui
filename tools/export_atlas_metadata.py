#!/usr/bin/env python3
"""
Export atlas metadata (from atlas.inl) to JSON and a simple BMFont .fnt text file.
Outputs:
 - demo/atlas_metadata.json
 - demo/atlas.fnt
"""
import re
import json
from pathlib import Path

p = Path(__file__).with_name('atlas.inl').read_text()

# parse width/height
m_w = re.search(r'ATLAS_WIDTH\s*=\s*(\d+)', p)
m_h = re.search(r'ATLAS_HEIGHT\s*=\s*(\d+)', p)
if not (m_w and m_h):
    raise SystemExit('Failed to find ATLAS_WIDTH/HEIGHT in atlas.inl')
W = int(m_w.group(1))
H = int(m_h.group(1))

# parse atlas entries
atlas_start = p.find('static mu_Rect atlas')
if atlas_start == -1:
    raise SystemExit('Failed to find atlas[] block')
atlas_end = p.find('};', atlas_start)
if atlas_end == -1:
    raise SystemExit('Failed to find end of atlas[] block')
block = p[atlas_start:atlas_end]
entries = re.findall(r'\[\s*([^\]]+?)\s*\]\s*=\s*\{\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\}', block)

out = Path(__file__).with_name('atlas_metadata.json')
meta = {'width': W, 'height': H, 'entries': []}
for name, xs, ys, ws, hs in entries:
    item = {'name': name.strip(), 'x': int(xs), 'y': int(ys), 'w': int(ws), 'h': int(hs)}
    # try to decode ATLAS_FONT+NNN => char code
    m_font = re.match(r'ATLAS_FONT\s*\+\s*(\d+)', item['name'])
    if m_font:
        item['char'] = int(m_font.group(1))
    meta['entries'].append(item)

out.write_text(json.dumps(meta, indent=2, ensure_ascii=False))
print('Wrote', out)

# write simple BMFont-style .fnt (text) minimal
fnt = Path(__file__).with_name('atlas.fnt')
lines = []
lines.append('info face="atlas" size=18')
lines.append(f'common lineHeight=18 scaleW={W} scaleH={H} pages=1')
lines.append('page id=0 file="atlas_extracted_rgba.png"')
lines.append('chars count=%d' % len(meta['entries']))
for e in meta['entries']:
    if 'char' not in e:
        # skip non-font entries (icons)
        continue
    # id x y width height xoffset yoffset xadvance page chnl
    lines.append('char id=%d   x=%d   y=%d   width=%d   height=%d   xoffset=0   yoffset=0   xadvance=%d   page=0   chnl=0' % (
        e['char'], e['x'], e['y'], e['w'], e['h'], e['w']))

fnt.write_text('\n'.join(lines))
print('Wrote', fnt)
