#!/usr/bin/env bash
# The verifier: the only party that may say a change passes.
# Agents propose; this script concludes. See docs/agents.md.
#
# Usage: scripts/verify.sh [stage...]
#   stages: engine  native  godot  routes  captures  quick  all (default: all)
#   quick = engine + native;  all = engine native godot routes (captures need a display)
# Each step writes build/verify/<step>.log; the summary is build/verify/last.json.
# Exit status is 0 only if every step that ran passed.
set -uo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
out="$root/build/verify"
mkdir -p "$out"
local_libs="$root/local_deps/usr/lib/x86_64-linux-gnu"
godot_bin="${GODOT_BIN:-$root/.tools/godot/Godot_v4.7.2-stable_linux.x86_64}"
jobs="${UMM_BUILD_JOBS:-$(nproc)}"

declare -a results=()
failed=0

step() {  # step <name> <command...>
    local name="$1"; shift
    local log="$out/$name.log" t0=$SECONDS status
    ( cd "$root" && "$@" ) >"$log" 2>&1
    status=$?
    local secs=$((SECONDS - t0))
    if [[ $status -eq 0 ]]; then
        printf 'PASS  %-28s %4ss\n' "$name" "$secs"
    else
        printf 'FAIL  %-28s %4ss  exit=%s  log=%s\n' "$name" "$secs" "$status" "${log#$root/}"
        tail -n 15 "$log" | sed 's/^/      | /'
        failed=1
    fi
    results+=("{\"step\":\"$name\",\"exit\":$status,\"seconds\":$secs,\"log\":\"${log#$root/}\"}")
}

engine() {
    step engine_build env LD_LIBRARY_PATH="$local_libs" make -C build -j"$jobs"
    step engine_tests ./build/raids_tests
}

native() {
    step native_ctest ./scripts/build_godot_combat.sh --test
}

godot_prepare() {
    [[ -n "${_godot_ready:-}" ]] && return 0
    step godot_prepare bash -c "./scripts/run_godot.sh --prepare && '$godot_bin' --headless --path godot --editor --import --quiet"
    _godot_ready=1
}

godot() {
    godot_prepare
    local t
    for t in art_smoke smoke combat_smoke demo_smoke expanded_demo_smoke fieldworks_smoke \
             north_theme_smoke rookwatch_smoke story_effects_smoke tactics_smoke; do
        step "godot_$t" timeout 600 "$godot_bin" --headless --audio-driver Dummy --path godot --script "res://tests/$t.gd"
    done
}

routes() {
    native_built=1
    [[ -x "$root/godot/native/build/demo_bot" ]] || step native_build ./scripts/build_godot_combat.sh
    local m
    for m in conquest bypass slow escort; do
        step "route_$m" timeout 300 ./godot/native/build/demo_bot . "$m" 1
    done
}

captures() {  # screenshots for the playtester; needs a display, never a pass/fail on looks
    godot_prepare
    local dir="$root/godot/artifacts/verify"
    mkdir -p "$dir"
    local t
    for t in intro_capture level_one_capture north_capture tactics_capture town_capture; do
        step "capture_$t" timeout 300 "$godot_bin" --audio-driver Dummy --position 4000,4000 --path godot --script "res://tests/$t.gd" -- "$dir"
    done
    echo "Screenshots: ${dir#$root/}/"
}

stages=("$@")
[[ ${#stages[@]} -eq 0 ]] && stages=(all)
for s in "${stages[@]}"; do
    case "$s" in
        engine) engine ;;
        native) native ;;
        godot) godot ;;
        routes) routes ;;
        captures) captures ;;
        quick) engine; native ;;
        all) engine; native; godot; routes ;;
        *) echo "Unknown stage: $s (engine native godot routes captures quick all)" >&2; exit 2 ;;
    esac
done

commit="$(git -C "$root" rev-parse --short HEAD 2>/dev/null || echo none)"
dirty="$(git -C "$root" status --porcelain 2>/dev/null | wc -l)"
{
    printf '{"commit":"%s","dirty_files":%s,"date":"%s","stages":"%s","passed":%s,"steps":[' \
        "$commit" "$dirty" "$(date -Iseconds)" "${stages[*]}" "$([[ $failed -eq 0 ]] && echo true || echo false)"
    (IFS=,; printf '%s' "${results[*]}")
    printf ']}\n'
} >"$out/last.json"

echo
if [[ $failed -eq 0 ]]; then echo "VERIFIED: every step passed (${stages[*]})"; else echo "NOT VERIFIED: see FAIL lines above"; fi
exit $failed
