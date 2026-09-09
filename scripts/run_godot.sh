#!/usr/bin/env bash
# Stage shared assets without changing the C++ game's originals.
set -euo pipefail
project_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
godot_project="$project_root/godot"
mkdir -p "$godot_project/content/textures" "$godot_project/content/maps"
cp -Ru "$project_root/assets/textures/." "$godot_project/content/textures/"
cp -u "$project_root/data/maps/default.json" "$godot_project/content/maps/default.json"

if [[ "${1:-}" == "--prepare" ]]; then
    echo "Shared assets staged. Open godot/project.godot in Godot 4.7 or newer."
    exit 0
fi

godot_bin="${GODOT_BIN:-}"
if [[ -z "$godot_bin" ]]; then
    for candidate in "$project_root/.tools/godot/Godot_v4.7.2-stable_linux.x86_64" godot godot4; do
        if command -v "$candidate" >/dev/null 2>&1; then
            godot_bin="$candidate"
            break
        fi
    done
fi
if [[ -z "$godot_bin" ]]; then
    echo "Godot is missing. Install Godot 4.7+, or set GODOT_BIN to its executable." >&2
    exit 1
fi

# Import before launch so a fresh checkout also works from the command line.
"$godot_bin" --headless --path "$godot_project" --editor --import --quiet
exec "$godot_bin" --path "$godot_project" "$@"
