#!/bin/sh
# Emit TAP for Python syntax validation with compileall.

set -eu

echo "1..1"

src_root=$1
python_bin=$2

cd "$src_root"
if test -z "$python_bin"; then
    python_bin=python3
fi
if ! command -v "$python_bin" >/dev/null 2>&1; then
    python_bin=python
fi
if ! command -v "$python_bin" >/dev/null 2>&1; then
    echo "ok 1 # SKIP python interpreter not found"
    exit 0
fi
find python tests/bindings/python tests/_static/python -name '*.py' \
    -exec "$python_bin" -m compileall -q {} +
echo "ok 1 - python compileall"
