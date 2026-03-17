#!/usr/bin/env python3
import pixellab
import os

secret = "29752808-5165-411f-b560-dd07920e3e91"

client = pixellab.Client(secret=secret)

response = client.generate_image_pixflux(
    description="castle, pixel art, fantasy, sandy desert, top-down view, medieval fortress with towers and walls, golden sand background",
    image_size={"width": 128, "height": 128},
    no_background=True,
)

img = response.image.pil_image()
output_path = "assets/textures/objects/castle.png"
img.save(output_path)
print(f"Saved castle sprite to {output_path}")
