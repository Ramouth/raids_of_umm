#!/usr/bin/env bash
# Stage shared assets without changing the C++ game's originals.
set -euo pipefail
project_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
godot_project="$project_root/godot"
mkdir -p "$godot_project/content/textures" "$godot_project/content/maps" "$godot_project/content/data"
cp -Ru "$project_root/assets/textures/." "$godot_project/content/textures/"
for map in default old_passage old_passage.encounters old_passage.triggers; do
    cp -u "$project_root/data/maps/$map.json" "$godot_project/content/maps/$map.json"
done
for definition in units spells buildings items hero_tree; do
    cp -u "$project_root/data/$definition.json" "$godot_project/content/data/$definition.json"
done
"$project_root/scripts/build_godot_combat.sh"

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
