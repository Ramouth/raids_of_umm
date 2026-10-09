# Generates godot/themes/north/snow1.png (Art id feature.snow): 128x128, drawn at 64x64 and
# doubled, so one art pixel is 2 world units, the meadow's pixel scale at zoom 1.
# Calm lying snow: one soft white-grey with a sparse scatter of short wind ridges
# (a lit lip over a thin shadow, light from the upper left). Low contrast, 4 colours,
# no specks; broad light and shade come from the shader, in world space, so they never repeat. Seamless: map_view's SnowField shader
# repeats it in world space. Deterministic (seeded).
# Usage: python3 godot/themes/north/make_snow.py godot/themes/north/snow1.png
import math, random, sys
from PIL import Image

random.seed(11)
N = 64
BASE, SOFT, SHADE, LIP = (215, 222, 228), (203, 212, 220), (188, 199, 210), (236, 240, 243)

px = [[BASE] * N for _ in range(N)]

# Wind ridges: short shallow arcs running east-west, wrapped at the tile edge.
for _ in range(11):
    x0, y0 = random.randrange(N), random.randrange(N)
    length = random.randint(6, 13)
    bend = random.uniform(-1.5, 1.5)
    for i in range(length):
        x = (x0 + i) % N
        y = (y0 + round(math.sin(i / length * math.pi) * bend * 2)) % N
        px[(y - 1) % N][x] = LIP          # wind-polished crest, lit from above
        px[y][x] = SHADE if 1 < i < length - 2 else SOFT   # its lee, fading at the ends

img = Image.new("RGBA", (N, N))
for y in range(N):
    for x in range(N):
        img.putpixel((x, y), px[y][x] + (255,))
img.resize((128, 128), Image.NEAREST).save(sys.argv[1])
