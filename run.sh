#!/bin/bash
# Launch Raids of Umm'Natur from the project root.
# SDL_VIDEODRIVER=x11 — use XWayland on Wayland sessions to avoid
# the compositor dropping the connection when launched from a subprocess.
set -e
cd "$(dirname "$0")"
LD_LIBRARY_PATH="$(pwd)/local_deps/usr/lib/x86_64-linux-gnu" \
SDL_VIDEODRIVER=x11 \
./build/raids_of_umm "$@" 2>&1 | tee game.log
