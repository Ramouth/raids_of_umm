#!/usr/bin/env python3
import pixellab

secret = "29752808-5165-411f-b560-dd07920e3e91"

client = pixellab.Client(secret=secret)

response = client.generate_image_pixflux(
    description="castle interior view, Heroes of Might and Magic style, throne room, medieval fantasy, stone walls, banners, golden throne, wooden tables, warm torchlight, pixel art, game UI",
    image_size={"width": 320, "height": 200},
    no_background=False,
)

img = response.image.pil_image()
output_path = "assets/textures/buildings/castle_interior.png"
img.save(output_path)
print(f"Saved castle interior to {output_path}")
