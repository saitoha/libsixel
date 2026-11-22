#!/bin/sh

set -eu

# TAP passes absolute paths for fixtures and generated artifacts. Keep those
# paths visible in the guest and forward the test's per-command settings.
exec wasmtime run -S inherit-env --dir . \
    --dir "${TOP_SRCDIR:?}" --dir "${TOP_BUILDDIR:?}" "$@"
