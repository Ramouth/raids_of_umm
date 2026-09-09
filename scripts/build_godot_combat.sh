#!/usr/bin/env bash
set -euo pipefail
project_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
native_dir="$project_root/godot/native"
if [[ ! -f "$native_dir/build/CMakeCache.txt" || ! -f "$native_dir/build/.configured" || "$native_dir/CMakeLists.txt" -nt "$native_dir/build/.configured" || "$native_dir/build_profile.json" -nt "$native_dir/build/.configured" ]]; then
    cmake -S "$native_dir" -B "$native_dir/build" -DCMAKE_BUILD_TYPE=Debug
    cmake -E touch "$native_dir/build/.configured"
fi
cmake --build "$project_root/godot/native/build" --parallel "${UMM_BUILD_JOBS:-4}"
if [[ "${1:-}" == "--test" ]]; then
    ctest --test-dir "$project_root/godot/native/build" --output-on-failure
fi
