#!/bin/sh
# Verify refresh=1 matches T0 for every dither method.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" delta-refresh/0028_all_dithers || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU palette backend is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - refresh one matches t0 for all dithers"
    exit 0
}
echo "ok 1 - refresh one matches t0 for all dithers"
exit 0
