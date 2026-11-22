#!/bin/sh

set -eu

# TAP passes absolute paths for fixtures and generated artifacts. Keep those
# paths visible in the guest and forward the test's per-command settings.
# The trusted test programs also open /dev/null by name from inside WASI.
exec wasmtime run -S inherit-env --dir . --dir /dev \
    --dir "${TOP_SRCDIR:?}" --dir "${TOP_BUILDDIR:?}" "$@"
