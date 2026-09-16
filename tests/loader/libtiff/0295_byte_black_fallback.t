#!/bin/sh
# Policy: docs/loader/libtiff.md
# Policy: docs/loader/color-management.md
# Verify byte black fallback with independent source values.
set -eux
export SIXEL_LOADER_CMS_COLORSYNC_NATIVE_GAMMA=0
# Keep successful trace assertions compact; print the trace on failure.
set +x
result=0
trace=$(SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader/0295_byte_black_fallback "${TOP_SRCDIR}/tests/data/color-numeric" 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP libtiff or requested CMS is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - byte black fallback"
    exit 0
}
test "${trace#*"LSXCMS2|event=apply|"}" = "$trace" || {
    echo "$trace" >&2
    echo "not ok 1 - inapplicable ICC unexpectedly applied"
    exit 0
}
set -x
echo "ok 1 - byte black fallback "
exit 0
