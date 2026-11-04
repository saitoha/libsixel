#!/bin/sh
# Verify mask size protected hole.
# Policy: docs/encoder/output/encode-policy/README.md
set -eux

echo "1..1"
set -v

set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/pipeline/0091_mask_size_protected_hole"
status=$?
set -e
test "$status" -ne 77 || {
    echo "ok 1 - mask size protected hole # SKIP threads unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - mask size protected hole"
    exit 0
}
echo "ok 1 - mask size protected hole"
exit 0
