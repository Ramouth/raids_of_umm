#!/usr/bin/env python3
"""Local OpenAI image generation (Python standard library only)."""

import argparse
import base64
import json
import os
from pathlib import Path
import sys
import urllib.error
import urllib.request

ROOT = Path(__file__).resolve().parent.parent
KEY_FILE = ROOT / ".tools" / "openai_api_key"
DEFAULT_MODEL = "gpt-image-2.5-flare"


def request(key, endpoint, payload=None):
    req = urllib.request.Request(
        "https://api.openai.com/v1/" + endpoint,
        data=json.dumps(payload).encode() if payload is not None else None,
        headers={"Authorization": "Bearer " + key, "Content-Type": "application/json"},
    )
    try:
        with urllib.request.urlopen(req, timeout=300) as response:
            return json.load(response)
    except urllib.error.HTTPError as exc:
        # Do not print the response body: authentication errors may include key fragments.
        hints = {
            400: "Request rejected; check the model and image options.",
            401: "Authentication failed. Replace the credential with an OpenAI project API key.",
            403: "Access denied. Check project permissions and organization verification.",
            404: "Model or endpoint unavailable to this project.",
            429: "Quota or rate limit reached. Check API billing and retry later.",
        }
        raise RuntimeError(f"HTTP {exc.code}: {hints.get(exc.code, 'OpenAI API request failed.')}") from None
    except urllib.error.URLError:
        raise RuntimeError("Cannot reach OpenAI. Check your network connection.") from None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("check", help="Check authentication without generating an image")
    generate = commands.add_parser("generate", help="Generate one PNG (uses API credits)")
    generate.add_argument("prompt")
    generate.add_argument("--out", type=Path, required=True)
    generate.add_argument("--model", default=DEFAULT_MODEL)
    generate.add_argument("--size", choices=["1024x1024", "1536x1024", "1024x1536"], default="1024x1024")
    generate.add_argument("--quality", choices=["low", "medium", "high"], default="low")
    generate.add_argument("--transparent", action="store_true")
    args = parser.parse_args()

    try:
        key = os.environ.get("OPENAI_API_KEY", "").strip()
        if not key and KEY_FILE.exists():
            key = KEY_FILE.read_text().strip()
        if not key:
            raise RuntimeError(f"Set OPENAI_API_KEY or put your key in {KEY_FILE}.")
        if args.command == "check":
            result = request(key, "models")
            models = sorted(item["id"] for item in result.get("data", []) if item["id"].startswith("gpt-image"))
            print("Authentication succeeded.")
            print("Listed image models: " + (", ".join(models) or "none"))
            print("Image generation permissions and billing require a generation request to verify.")
            return 0

        if not args.prompt.strip():
            raise RuntimeError("Prompt must not be empty.")
        if args.out.suffix.lower() != ".png":
            raise RuntimeError("Output filename must end in .png.")
        if args.out.exists():
            raise RuntimeError(f"Output already exists: {args.out}. Choose a new filename.")
        args.out.parent.mkdir(parents=True, exist_ok=True)
        result = request(key, "images/generations", {
            "model": args.model, "prompt": args.prompt, "n": 1,
            "size": args.size, "quality": args.quality, "output_format": "png",
            "background": "transparent" if args.transparent else "opaque",
        })
        data = result.get("data", [])
        if not data or not data[0].get("b64_json"):
            raise RuntimeError("OpenAI returned no image data.")
        image = base64.b64decode(data[0]["b64_json"], validate=True)
        if not image.startswith(b"\x89PNG\r\n\x1a\n"):
            raise RuntimeError("OpenAI returned an unexpected image format.")
        with args.out.open("xb") as output:
            output.write(image)
        print(f"Saved {args.out}")
        return 0
    except (RuntimeError, OSError, ValueError) as exc:
        print(str(exc), file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
