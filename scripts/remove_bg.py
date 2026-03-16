#!/usr/bin/env python3
"""
Remove solid background from PixelLab-generated sprites using corner flood-fill.

Usage:
    python3 scripts/remove_bg.py assets/textures/objects/dungeon.png
    python3 scripts/remove_bg.py assets/textures/units/*.png
"""

import sys
from pathlib import Path
from PIL import Image
from collections import Counter, deque


def remove_bg(path: Path, tolerance: int = 30) -> None:
    img = Image.open(path).convert("RGBA")
    w, h = img.size
    pixels = img.load()

    # Sample background color from all 4 corners and pick most common.
    corners = [pixels[0, 0], pixels[w-1, 0], pixels[0, h-1], pixels[w-1, h-1]]
    bg = Counter(c[:3] for c in corners).most_common(1)[0][0]

    def similar(c1, c2):
        return all(abs(int(a) - int(b)) <= tolerance for a, b in zip(c1[:3], c2[:3]))

    visited: set = set()
    queue: deque = deque([(0, 0), (w-1, 0), (0, h-1), (w-1, h-1)])

    while queue:
        x, y = queue.popleft()
        if (x, y) in visited or not (0 <= x < w and 0 <= y < h):
            continue
        visited.add((x, y))
        c = pixels[x, y]
        if similar(c[:3], bg):
            pixels[x, y] = (c[0], c[1], c[2], 0)
            for dx, dy in [(1, 0), (-1, 0), (0, 1), (0, -1)]:
                nx, ny = x + dx, y + dy
                if (nx, ny) not in visited:
                    queue.append((nx, ny))

    img.save(path)
    print(f"  {path.name}: bg={bg}, {len(visited)} pixels removed")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python3 scripts/remove_bg.py <file> [file2 ...]")
        sys.exit(1)
    for arg in sys.argv[1:]:
        for p in Path(".").glob(arg) if "*" in arg else [Path(arg)]:
            remove_bg(p)
