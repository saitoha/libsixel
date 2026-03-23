#!/bin/sh
# Emit TAP for environment-variable docs consistency static check.

set -eu

echo "1..1"

src_root=$1
checker="$src_root/tests/docs/consistency/list_envvars.sh"

if test ! -f "$checker"; then
    echo "ok 1 # SKIP missing list_envvars.sh"
    exit 0
fi

tmpout=`mktemp "${TMPDIR:-/tmp}/libsixel-staticcheck-envvars-XXXXXX"`
cleanup() {
    rm -f "$tmpout"
}
trap cleanup EXIT HUP INT TERM

if sh "$checker" --check --source-root "$src_root" >"$tmpout" 2>&1; then
    echo "ok 1 - env help table matches source env vars"
    exit 0
fi

echo "not ok 1 - env help table matches source env vars"
sed 's/^/# /' "$tmpout"
exit 1
