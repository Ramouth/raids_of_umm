# Generates godot/themes/north/snow1.png (Art id feature.snow): 128x128, 7-colour cool palette,
# drawn at 64x64 and doubled so the pixels stay chunky. Deterministic (seeded).
# Usage: python3 godot/themes/north/make_snow.py godot/themes/north/snow1.png
import math, random, sys
from PIL import Image
random.seed(7)
N = 64
PAL = {"deep": (143,163,181), "shade": (180,197,211), "base": (212,223,231),
       "light": (232,239,243), "high": (247,250,251), "grass": (93,110,76), "stone": (107,111,115)}
# Smooth value noise on a periodic lattice
def lattice(n):
    return [[random.random() for _ in range(n)] for _ in range(n)]
L1, L2 = lattice(8), lattice(16)
def vnoise(L, n, x, y):
    fx, fy = x / N * n, y / N * n
    x0, y0 = int(fx) % n, int(fy) % n
    x1, y1 = (x0 + 1) % n, (y0 + 1) % n
    tx, ty = fx - int(fx), fy - int(fy)
    tx, ty = tx*tx*(3-2*tx), ty*ty*(3-2*ty)
    a = L[y0][x0]*(1-tx) + L[y0][x1]*tx
    b = L[y1][x0]*(1-tx) + L[y1][x1]*tx
    return a*(1-ty) + b*ty
def height(x, y):
    # Drifts run roughly east-west, swelling and thinning.
    d = math.sin(y * 0.21 + math.sin(x * 0.11) * 2.2 + vnoise(L1, 8, x, y) * 2.5)
    return d * 0.55 + vnoise(L1, 8, x, y) * 0.6 + vnoise(L2, 16, x, y) * 0.25
img = Image.new("RGBA", (N, N))
H = [[height(x, y) for x in range(N)] for y in range(N)]
for y in range(N):
    for x in range(N):
        h = H[y][x]
        # Upper-left light: slope facing up (h falls going down) is lit.
        slope = H[min(y+1, N-1)][x] - h + (H[y][min(x+1, N-1)] - h) * 0.5
        v = 0.6 + (h - 0.6) * 0.22 - slope * 1.3
        if v < 0.27: c = "deep"
        elif v < 0.47: c = "shade"
        elif v < 0.66: c = "base"
        elif v < 0.84: c = "light"
        else: c = "high"
        img.putpixel((x, y), PAL[c] + (255,))
# Sparse dark specks: grass blades and stones poking through, kept to the inner area.
for _ in range(4):
    x, y = random.randint(14, 49), random.randint(14, 49)
    kind = random.choice(["grass", "grass", "stone"])
    img.putpixel((x, y), PAL[kind] + (255,))
    if kind == "grass": img.putpixel((x + 1, y - 1), PAL[kind] + (255,))
    else: img.putpixel((x + 1, y), PAL["shade"] + (255,)); img.putpixel((x, y - 1), PAL["high"] + (255,))
img.resize((128, 128), Image.NEAREST).save(sys.argv[1])
