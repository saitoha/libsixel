#!/bin/sh
# Verify map all sixbit values.
# Policy: docs/encoder/output/encode-policy/README.md
set -eux

echo "1..1"
set -v

set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/pipeline/0093_map_all_sixbit_values"
status=$?
set -e
test "$status" -ne 77 || {
    printf '\n%s\n' "ok 1 - map all sixbit values # SKIP threads unavailable"
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' "not ok 1 - map all sixbit values"
    exit 0
}
printf '\n%s\n' "ok 1 - map all sixbit values"
exit 0
