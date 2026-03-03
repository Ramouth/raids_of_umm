#!/usr/bin/env python3
"""
PixelLab API client for Raids of Umm'Natur asset generation.

Usage:
    python3 scripts/pixellab_client.py balance
    python3 scripts/pixellab_client.py pixflux "desert warrior on horseback" --size 64x64 --out assets/textures/units/warrior.png
    python3 scripts/pixellab_client.py bitforge "castle tower, egyptian style" --size 128x128 --out assets/textures/buildings/tower.png
    python3 scripts/pixellab_client.py animate "desert rider galloping" --action walk --direction south --out assets/textures/units/rider_anim.png
"""

import sys
import json
import base64
import argparse
from pathlib import Path

import requests

# ── Config ────────────────────────────────────────────────────────────────────

REPO_ROOT = Path(__file__).resolve().parent.parent
KEY_FILE  = REPO_ROOT / "assets" / "pixellab"
BASE_URL  = "https://api.pixellab.ai/v1"


def load_api_key() -> str:
    if not KEY_FILE.exists():
        sys.exit(f"Key file not found: {KEY_FILE}")
    text = KEY_FILE.read_text().strip()
    for line in text.splitlines():
        if line.startswith("secret:"):
            return line.split(":", 1)[1].strip()
    sys.exit(f"Could not parse secret from {KEY_FILE}")


# ── Client ────────────────────────────────────────────────────────────────────

class PixelLabClient:
    def __init__(self, api_key: str):
        self.session = requests.Session()
        self.session.headers.update({
            "Authorization": f"Bearer {api_key}",
            "Content-Type": "application/json",
        })

    def _post(self, endpoint: str, payload: dict) -> dict:
        r = self.session.post(f"{BASE_URL}/{endpoint}", json=payload)
        r.raise_for_status()
        return r.json()

    def _get(self, endpoint: str) -> dict:
        r = self.session.get(f"{BASE_URL}/{endpoint}")
        r.raise_for_status()
        return r.json()

    def balance(self) -> dict:
        return self._get("balance")

    def generate_pixflux(
        self,
        description: str,
        width: int = 64,
        height: int = 64,
        guidance: float = 7.0,
        transparent: bool = False,
    ) -> bytes:
        """Text-to-pixel-art via Pixflux model."""
        payload = {
            "description": description,
            "image_size": {"width": width, "height": height},
            "text_guidance_scale": guidance,
            "transparent_background": transparent,
            "no_background": transparent,
        }
        data = self._post("generate-image-pixflux", payload)
        return _decode_image(data)

    def generate_bitforge(
        self,
        description: str,
        width: int = 64,
        height: int = 64,
        style_strength: int = 50,
        transparent: bool = False,
    ) -> bytes:
        """Text-to-pixel-art via Bitforge model (style transfer capable)."""
        payload = {
            "description": description,
            "image_size": {"width": width, "height": height},
            "style_strength": style_strength,
            "transparent_background": transparent,
        }
        data = self._post("generate-image-bitforge", payload)
        return _decode_image(data)

    def animate(
        self,
        description: str,
        action: str = "walk",
        direction: str = "south",
        view: str = "side",
    ) -> bytes:
        """Generate a 4-frame animation strip (64x64 per frame)."""
        payload = {
            "description": description,
            "action": action,
            "direction": direction,
            "view": view,
        }
        data = self._post("animate-with-text", payload)
        return _decode_image(data)


# ── Helpers ───────────────────────────────────────────────────────────────────

def _decode_image(response: dict) -> bytes:
    """Extract raw PNG bytes from a PixelLab response."""
    # Response schema: { "image": { "url": "...", "type": "base64", "data": "..." } }
    # or just { "image": "<base64string>" }
    img = response.get("image") or response.get("data") or response
    if isinstance(img, dict):
        if "base64" in img:
            return base64.b64decode(img["base64"])
        if img.get("type") == "base64" or "data" in img:
            return base64.b64decode(img.get("base64") or img["data"])
        if "url" in img:
            r = requests.get(img["url"])
            r.raise_for_status()
            return r.content
    if isinstance(img, str):
        return base64.b64decode(img)
    raise ValueError(f"Unrecognised image response shape: {list(response.keys())}")


def _parse_size(s: str) -> tuple[int, int]:
    parts = s.lower().replace("x", " ").replace(",", " ").split()
    if len(parts) != 2:
        sys.exit(f"Invalid size '{s}', expected WxH (e.g. 64x64)")
    return int(parts[0]), int(parts[1])


def _save(png: bytes, out: Path):
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(png)
    print(f"Saved {len(png)} bytes → {out}")


# ── CLI ───────────────────────────────────────────────────────────────────────

def main():
    p = argparse.ArgumentParser(description="PixelLab asset generator for Raids of Umm'Natur")
    sub = p.add_subparsers(dest="cmd", required=True)

    # balance
    sub.add_parser("balance", help="Show remaining API credits")

    # pixflux
    pf = sub.add_parser("pixflux", help="Generate pixel art (Pixflux model)")
    pf.add_argument("description")
    pf.add_argument("--size", default="64x64")
    pf.add_argument("--guidance", type=float, default=7.0)
    pf.add_argument("--transparent", action="store_true")
    pf.add_argument("--out", required=True)

    # bitforge
    bf = sub.add_parser("bitforge", help="Generate pixel art (Bitforge model)")
    bf.add_argument("description")
    bf.add_argument("--size", default="64x64")
    bf.add_argument("--style-strength", type=int, default=50)
    bf.add_argument("--transparent", action="store_true")
    bf.add_argument("--out", required=True)

    # animate
    an = sub.add_parser("animate", help="Generate 4-frame animation strip")
    an.add_argument("description")
    an.add_argument("--action", default="walk")
    an.add_argument("--direction", default="south")
    an.add_argument("--view", default="side")
    an.add_argument("--out", required=True)

    args = p.parse_args()
    client = PixelLabClient(load_api_key())

    if args.cmd == "balance":
        data = client.balance()
        print(json.dumps(data, indent=2))

    elif args.cmd == "pixflux":
        w, h = _parse_size(args.size)
        png = client.generate_pixflux(args.description, w, h, args.guidance, args.transparent)
        _save(png, Path(args.out))

    elif args.cmd == "bitforge":
        w, h = _parse_size(args.size)
        png = client.generate_bitforge(args.description, w, h, args.style_strength, args.transparent)
        _save(png, Path(args.out))

    elif args.cmd == "animate":
        png = client.animate(args.description, args.action, args.direction, args.view)
        _save(png, Path(args.out))


if __name__ == "__main__":
    main()
