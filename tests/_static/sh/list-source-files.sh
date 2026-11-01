#!/bin/sh
# Static checks already require Python; runtime case discovery uses shell globs.
set -eu
exec "${PYTHON_STATICCHECK:-python3}" \
    "${0%/*}/../python/source_files.py" "$@"
