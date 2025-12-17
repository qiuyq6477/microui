#!/usr/bin/env python3
"""
Pack a single-channel atlas image + metadata JSON back into atlas.inl format.
Usage:
  python pack_atlas.py --image atlas_extracted.png --meta atlas_metadata.json --out atlas_new.inl

Notes:
 - Input image may be L (grayscale) or RGBA (uses alpha channel). If RGBA and no alpha, will use luminance.
 - Metadata JSON format as produced by export_atlas_metadata.py
"""
import argparse
import json
from pathlib import Path
from PIL import Image

parser = argparse.ArgumentParser()
parser.add_argument('--image', required=True)
parser.add_argument('--meta', required=True)
parser.add_argument('--out', default='atlas_new.inl')
args = parser.parse_args()

img = Image.open(args.image)
# prefer alpha if present, else convert to L
if img.mode == 'RGBA' or img.mode == 'LA':
    # get alpha channel
    a = img.split()[-1]
    channel = a.convert('L')
else:
    channel = img.convert('L')

meta = json.loads(Path(args.meta).read_text())
W = meta['width']
H = meta['height']
if channel.size != (W, H):
    raise SystemExit(f'Image size {channel.size} does not match meta {W}x{H}')

# create raw bytes
raw = channel.tobytes()

# build C initializer lines
lines = []
lines.append('enum { ATLAS_WHITE = MU_ICON_MAX, ATLAS_FONT };')
lines.append(f'enum {{ ATLAS_WIDTH = {W}, ATLAS_HEIGHT = {H} }};')
lines.append('')
lines.append(f'static unsigned char atlas_texture[ATLAS_WIDTH * ATLAS_HEIGHT] = {{')
# format bytes as 12 per line hex
for i in range(0, len(raw), 12):
    chunk = raw[i:i+12]
    line = '  ' + ', '.join('0x%02x' % b for b in chunk) + ','
    lines.append(line)
lines.append('};')
lines.append('')
# atlas rects
lines.append('static mu_Rect atlas[] = {')
for e in meta['entries']:
    name = e['name']
    lines.append(f'  [ {name} ] = {{ {e["x"]}, {e["y"]}, {e["w"]}, {e["h"]} }},')
lines.append('};')

out = Path(args.out)
out.write_text('\n'.join(lines))
print('Wrote', out)
