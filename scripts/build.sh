#!/usr/bin/env bash
set -euo pipefail

PRESET="${1:-debug}"

echo "==> Configuring preset: $PRESET"
cmake --preset "$PRESET"

echo "==> Building"
cmake --build "build/$PRESET" --parallel "$(nproc)"

if [[ "$PRESET" == "debug" ]]; then
    echo "==> Running unit tests"
    ctest --test-dir "build/$PRESET" --output-on-failure
fi

echo "==> Done."
