#!/usr/bin/env python3
"""
fix_hex_fill.py — Post-process PixelLab terrain tiles.

Two operations:
  1. FILL: For pixels inside the theoretical hex boundary that are transparent
     (holes/gaps in the tile), fill them with the nearest opaque neighbour.
     This ensures tiles cover the full hexagon when rendered.

  2. TINT-PREP (--desaturate): Create a _neutral.png companion tile — a greyscale
     version of the base tile — so it can be used with the tint mode in hex.frag
     (u_TintMode=1: color = texColor * v_Color * 2.0).  These neutral variants
     take on the exact hue of any terrain's base colour at paint time.

Usage:
  python scripts/fix_hex_fill.py                    # fill all tiles in-place
  python scripts/fix_hex_fill.py --desaturate        # also write _neutral.png per tile
  python scripts/fix_hex_fill.py --dry-run           # preview without writing
  python scripts/fix_hex_fill.py assets/textures/terrain/sand/sand0.png   # single file
"""

import argparse
import math
import sys
from pathlib import Path
from PIL import Image, ImageFilter

TILE_SIZE   = 128
HEX_RADIUS  = 60       # circumradius in pixels (vertex-to-centre); leaves 4px safety margin
HEX_CX      = 63.5
HEX_CY      = 63.5
ALPHA_OPAQUE = 128     # pixels with alpha >= this are considered opaque


# ──────────────────────────────────────────────────────────────────────────────
# Hex geometry helpers
# ──────────────────────────────────────────────────────────────────────────────

def _hex_verts(cx=HEX_CX, cy=HEX_CY, r=HEX_RADIUS):
    """Return the 6 vertices of a flat-top regular hexagon."""
    return [
        (cx + r * math.cos(math.radians(i * 60)),
         cy + r * math.sin(math.radians(i * 60)))
        for i in range(6)
    ]


def _build_hex_mask(size=TILE_SIZE):
    """Return a boolean 2-D list: True = inside hex boundary."""
    verts = _hex_verts()
    n     = len(verts)
    mask  = [[False] * size for _ in range(size)]

    for py in range(size):
        for px in range(size):
            inside = True
            for i in range(n):
                x1, y1 = verts[i]
                x2, y2 = verts[(i + 1) % n]
                # For CCW winding, point is inside all half-planes when cross <= 0
                cross = (x2 - x1) * (py - y1) - (y2 - y1) * (px - x1)
                if cross > 0:
                    inside = False
                    break
            mask[py][px] = inside

    return mask


# Pre-compute hex mask once for the standard 128×128 tile size
HEX_MASK = _build_hex_mask()


# ──────────────────────────────────────────────────────────────────────────────
# Fill pass
# ──────────────────────────────────────────────────────────────────────────────

def fill_interior_transparent(img: Image.Image) -> tuple[Image.Image, int]:
    """
    Fill transparent pixels that sit *inside* the hex boundary with the colour
    of their nearest opaque neighbour.  Returns (new_image, num_pixels_filled).
    """
    if img.mode != "RGBA":
        img = img.convert("RGBA")

    pixels   = list(img.getdata())
    w, h     = img.size
    changed  = [False] * (w * h)
    filled   = 0

    def idx(x, y): return y * w + x

    # Iteratively expand opaque region into interior transparent pixels.
    # Each pass fills pixels adjacent to already-opaque ones.
    max_iters = 64   # sufficient for a 128px tile
    for _ in range(max_iters):
        new_fill = False
        for py in range(h):
            for px in range(w):
                i = idx(px, py)
                if pixels[i][3] >= ALPHA_OPAQUE:
                    continue  # already opaque
                if not HEX_MASK[py][px]:
                    continue  # outside hex — keep transparent

                # Look for an opaque neighbour (8-connected)
                for dy in (-1, 0, 1):
                    for dx in (-1, 0, 1):
                        nx_, ny_ = px + dx, py + dy
                        if 0 <= nx_ < w and 0 <= ny_ < h:
                            ni = idx(nx_, ny_)
                            if pixels[ni][3] >= ALPHA_OPAQUE:
                                r, g, b, _ = pixels[ni]
                                pixels[i]  = (r, g, b, 255)
                                changed[i] = True
                                new_fill   = True
                                filled    += 1
                                break
                    else:
                        continue
                    break

        if not new_fill:
            break  # converged

    result = img.copy()
    result.putdata(pixels)
    return result, filled


# ──────────────────────────────────────────────────────────────────────────────
# Desaturate pass (tint-prep)
# ──────────────────────────────────────────────────────────────────────────────

def desaturate(img: Image.Image) -> Image.Image:
    """
    Return a greyscale (luminance-preserving) version of img, keeping the alpha
    channel.  These _neutral tiles work with u_TintMode=1 in hex.frag:
       color = texColor * v_Color * 2.0
    so the terrain base colour fully controls the final hue.
    """
    if img.mode != "RGBA":
        img = img.convert("RGBA")

    r_img, g_img, b_img, a_img = img.split()
    # Rec. 601 luminance weights
    grey = Image.merge("RGB", (r_img, g_img, b_img)).convert("L")
    # Re-combine as greyscale RGB with original alpha
    grey_rgb = Image.merge("RGB", (grey, grey, grey))
    grey_rgba = Image.merge("RGBA", (*grey_rgb.split(), a_img))
    return grey_rgba


# ──────────────────────────────────────────────────────────────────────────────
# Main
# ──────────────────────────────────────────────────────────────────────────────

def process_file(path: Path, desaturate_flag: bool, dry_run: bool) -> None:
    try:
        img = Image.open(path).convert("RGBA")
    except Exception as e:
        print(f"  SKIP {path.name}: {e}")
        return

    if img.size != (TILE_SIZE, TILE_SIZE):
        print(f"  SKIP {path.name}: size {img.size} != {TILE_SIZE}×{TILE_SIZE}")
        return

    filled_img, n_filled = fill_interior_transparent(img)

    if not dry_run:
        filled_img.save(path)
    print(f"  {'[dry]' if dry_run else 'FIXED'} {path.name:30s}  filled {n_filled} interior transparent pixels")

    if desaturate_flag and not path.stem.endswith("_neutral"):
        neutral_path = path.with_stem(path.stem + "_neutral")
        neutral_img  = desaturate(filled_img)
        if not dry_run:
            neutral_img.save(neutral_path)
        print(f"  {'[dry]' if dry_run else 'WROTE'} {neutral_path.name}")


def main():
    parser = argparse.ArgumentParser(description="Fix hex terrain tile fill and/or create neutral tint variants")
    parser.add_argument("files",       nargs="*", help="Specific PNG files to process (default: all terrain tiles)")
    parser.add_argument("--desaturate",action="store_true", help="Also write a <name>_neutral.png desaturated companion")
    parser.add_argument("--dry-run",   action="store_true", help="Report what would change without writing files")
    args = parser.parse_args()

    if args.files:
        paths = [Path(f) for f in args.files]
    else:
        terrain_root = Path("assets/textures/terrain")
        paths = sorted(terrain_root.rglob("*.png"))
        # Skip companion tiles already generated, and non-128px tiles (road, battle, etc.)
        paths = [p for p in paths if "_neutral" not in p.stem]

    if not paths:
        print("No PNG files found.")
        sys.exit(0)

    print(f"Processing {len(paths)} tile(s)  [dry-run={args.dry_run}]")
    for p in paths:
        process_file(p, args.desaturate, args.dry_run)

    print("Done.")


if __name__ == "__main__":
    main()
