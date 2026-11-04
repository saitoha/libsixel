#!/bin/sh
# Verify bucket equal span tie.
# Policy: docs/encoder/output/encode-policy/README.md
set -eux

echo "1..1"
set -v

set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/pipeline/0065_bucket_equal_span_tie"
status=$?
set -e
test "$status" -ne 77 || {
    echo "ok 1 - bucket equal span tie # SKIP threads unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - bucket equal span tie"
    exit 0
}
echo "ok 1 - bucket equal span tie"
exit 0
