"""Crop Virtuoso exports to their content (colored pixels; grid dots are gray)."""
import os, sys
from PIL import Image
import numpy as np
src, dst = sys.argv[1], sys.argv[2]
os.makedirs(dst, exist_ok=True)
for f in sorted(os.listdir(src)):
    if not f.endswith('.png'): continue
    im = Image.open(os.path.join(src, f)).convert('RGB'); a = np.asarray(im).astype(int)
    col = (a.max(2) - a.min(2)) > 60
    ys, xs = np.nonzero(col)
    m = 25
    box = (max(xs.min()-m, 0), max(ys.min()-m, 0), min(xs.max()+m, im.width), min(ys.max()+m, im.height))
    im.crop(box).save(os.path.join(dst, f))
    print(f, im.size, '->', box)
