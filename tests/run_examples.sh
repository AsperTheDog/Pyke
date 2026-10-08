#!/bin/bash
# Generates, configures, builds and tests every project in examples/projects with a real CMake.
# usage: [PYKE_OFFLINE=1] tests/run_examples.sh <path-to-pyke-binary>
# Projects that fetch from GitHub are skipped when PYKE_OFFLINE=1.
set -u
PYKE="$(realpath "${1:?usage: run_examples.sh <pyke-binary>}")"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
fail=0
for dir in "$ROOT"/examples/projects/*/; do
    name="$(basename "$dir")"
    if [ -n "${CI:-}" ] && [ -f "$dir/.skip-in-ci" ]; then echo "SKIP  $name (needs a newer toolchain than CI has)"; continue; fi
    if [ -n "${PYKE_OFFLINE:-}" ] && grep -q "^from github" "$dir/app.pyke"; then echo "SKIP  $name (needs network)"; continue; fi
    cp -r "$dir" "$WORK/$name"
    (
        cd "$WORK/$name" || exit 1
        "$PYKE" app.pyke . >gen.log 2>&1 || { cat gen.log; exit 1; }
        cmake -S . -B _b >cfg.log 2>&1 || { cat cfg.log; exit 1; }
        cmake --build _b --config Release >build.log 2>&1 || { tail -30 build.log; exit 1; }
        ctest --test-dir _b -C Release --output-on-failure >test.log 2>&1 || { cat test.log; exit 1; }
        if [ -d consumer ]; then
            cmake --install _b --config Release --prefix "$PWD/_inst" >inst.log 2>&1 || { cat inst.log; exit 1; }
            cmake -S consumer -B _cb -DCMAKE_PREFIX_PATH="$PWD/_inst" >ccfg.log 2>&1 || { cat ccfg.log; exit 1; }
            cmake --build _cb --config Release >cbuild.log 2>&1 || { tail -30 cbuild.log; exit 1; }
            ctest --test-dir _cb -C Release --output-on-failure >ctest.log 2>&1 || { cat ctest.log; exit 1; }
        fi
        # a second run must not touch anything
        "$PYKE" app.pyke . 2>&1 | grep -q "0 written" || { echo "regeneration was not idempotent"; exit 1; }
    ) && echo "PASS  $name" || { echo "FAIL  $name"; fail=1; }
done
exit $fail
