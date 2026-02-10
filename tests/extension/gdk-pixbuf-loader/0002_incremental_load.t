#!/bin/sh
# TAP wrapper that dispatches to the unified C test runner.

set -eu

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_feature_available "HAVE_GDK_PIXBUF2" "gdk_pixbuf_loader" \
    "gdk-pixbuf loader"

set +e
loader_output=$(run_test_runner "gdk-pixbuf-loader/${test_name}" 2>&1)
rc=$?
set -e
printf '%s' "${loader_output}" >&2

echo "1..1"
set -v

if [ "${rc}" -eq 0 ]; then
    echo "ok 1 - gdk-pixbuf-loader/${test_name}"
elif [ "${rc}" -eq 77 ]; then
    echo "ok 1 - gdk-pixbuf-loader/${test_name} # SKIP unavailable"
else
    echo "not ok 1 - gdk-pixbuf-loader/${test_name}"
fi
