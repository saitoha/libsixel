#!/bin/sh
set -eu

if [ "${PYTHON:-}" = "" ]; then
  echo "SKIP: Python interpreter not configured" >&2
  exit 77
fi

script_dir=$(cd "$(dirname "$0")" && pwd)
top_builddir=${TOP_BUILDDIR:?TOP_BUILDDIR is not set}
libdir="$top_builddir/src/.libs"

export LD_LIBRARY_PATH="$libdir${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export DYLD_LIBRARY_PATH="$libdir${DYLD_LIBRARY_PATH:+:$DYLD_LIBRARY_PATH}"
export PYTHONPATH="${script_dir%/*}${PYTHONPATH:+:$PYTHONPATH}"

exec "$PYTHON" "$script_dir/test_libsixel.py"
