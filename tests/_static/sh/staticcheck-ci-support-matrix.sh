#!/bin/sh
# Emit TAP for CI configuration and documentation inventory parity.

set -eu

src_root=$1
python_bin=$2
generator=$src_root/tests/_static/python/generate_ci_support_matrix.py
snapshot=$src_root/docs/ci/local-jobs.tsv
inventory=$src_root/docs/ci/support-matrix.md
local_catalog=${LIBSIXEL_LOCAL_CI_JOBS-}

echo "1..1"

test -n "$python_bin" || {
    echo "ok 1 # SKIP python is not configured"
    exit 0
}

test -x "$python_bin" || command -v "$python_bin" >/dev/null 2>&1 || {
    echo "ok 1 # SKIP python is not available: $python_bin"
    exit 0
}

test -n "$local_catalog" || {
    candidate=$src_root/../codex-work/libsixel-ci/srv/misc/jobs.tsv
    test ! -f "$candidate" || local_catalog=$candidate
}

set -- "$python_bin" "$generator" \
    --root "$src_root" \
    --local-snapshot "$snapshot" \
    --check "$inventory"

test -z "$local_catalog" || set -- "$@" \
    --check-local-catalog "$local_catalog"

"$@" || {
    echo "not ok 1 - CI support matrix matches CI configuration"
    exit 1
}

test -z "$local_catalog" || \
    echo "# checked local CI catalog: $local_catalog"
test -n "$local_catalog" || \
    echo "# local CI catalog unavailable; checked tracked snapshot"
echo "ok 1 - CI support matrix matches CI configuration"
