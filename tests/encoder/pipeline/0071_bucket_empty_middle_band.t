#!/bin/sh
# Verify bucket empty middle band.
# Policy: docs/encoder/output/encode-policy/README.md
set -eux

echo "1..1"
set -v

set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/pipeline/0071_bucket_empty_middle_band"
status=$?
set -e
test "$status" -ne 77 || {
    echo "ok 1 - bucket empty middle band # SKIP threads unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - bucket empty middle band"
    exit 0
}
echo "ok 1 - bucket empty middle band"
exit 0
