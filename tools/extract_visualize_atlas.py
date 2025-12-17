#!/usr/bin/env python3
"""
Extract and visualize single-channel atlas from demo/atlas.inl
Saves:
 - demo/atlas_extracted.png      (grayscale image)
 - demo/atlas_extracted_rgba.png (white RGB + alpha from atlas)
 - demo/atlas_parts/*.png        (each atlas rect saved as RGBA)
"""
import argparse
import re
from pathlib import Path
from PIL import Image

parser = argparse.ArgumentParser()
parser.add_argument('--infile', default='atlas.inl', help='atlas .inl file name in demo/')
args = parser.parse_args()

p = Path(__file__).with_name(args.infile).read_text()

# width/height
m_w = re.search(r'ATLAS_WIDTH\s*=\s*(\d+)', p)
m_h = re.search(r'ATLAS_HEIGHT\s*=\s*(\d+)', p)
if not (m_w and m_h):
    raise SystemExit('Failed to find ATLAS_WIDTH/HEIGHT in atlas.inl')
w = int(m_w.group(1))
h = int(m_h.group(1))

# atlas_texture bytes (robust parse)
start = p.find('static unsigned char atlas_texture')
if start == -1:
    raise SystemExit('Failed to find atlas_texture declaration')
# find the terminator '};' after the declaration
end_marker = 'static mu_Rect atlas'
end = p.find(end_marker, start)
if end == -1:
    raise SystemExit(f'Failed to find end marker ({end_marker}) for atlas_texture')
block = p[start:end]
vals = re.findall(r'0x[0-9A-Fa-f]+|\d+', block)
data = bytes(int(v, 0) for v in vals)
if len(data) != w * h:
    if len(data) < w * h:
        print(f'Warning: initializer has {len(data)} values, padding with zeros to {w*h}')
        data = data + bytes((w * h) - len(data))
    else:
        raise SystemExit(f'Unexpected data length: {len(data)} != {w}*{h} ({w*h})')

# save grayscale image (L)
img = Image.frombytes('L', (w, h), data)
out = Path(__file__).with_name('atlas_extracted.png')
img.save(out)
print('Saved', out)

# save as white RGB + alpha using the channel as alpha
rgba = Image.new('RGBA', img.size, (255, 255, 255, 0))
rgba.putalpha(img)
out2 = Path(__file__).with_name('atlas_extracted_rgba.png')
rgba.save(out2)
print('Saved', out2)

# parse atlas[] rects (robust)
atlas_start = p.find('static mu_Rect atlas')
if atlas_start == -1:
    print('Warning: atlas[] block not found, skipping parts extraction')
else:
    atlas_end = p.find('};', atlas_start)
    if atlas_end == -1:
        print('Warning: atlas[] end not found, skipping parts extraction')
    else:
        block = p[atlas_start:atlas_end]
        entries = re.findall(r'\[\s*([^\]]+?)\s*\]\s*=\s*\{\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\}', block)
        outdir = Path(__file__).with_name('atlas_parts')
        outdir.mkdir(exist_ok=True)
        count = 0
        for name, xs, ys, ws, hs in entries:
            x, y, ww, hh = map(int, (xs, ys, ws, hs))
            crop = img.crop((x, y, x + ww, y + hh))
            # compose as RGBA (white with alpha)
            rimg = Image.new('RGBA', crop.size, (255, 255, 255, 0))
            rimg.putalpha(crop)
            safe = name.strip().replace(' ', '_').replace('+', '_plus_').replace('-', '_')
            path = outdir / f'{safe}_{x}_{y}_{ww}x{hh}.png'
            rimg.save(path)
            count += 1
        print(f'Extracted {count} parts to', outdir)

print('Done.')
