#!/bin/sh
# Verify mask sparse keycolor.
# Policy: docs/encoder/output/encode-policy/README.md
set -eux

echo "1..1"
set -v

set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/pipeline/0090_mask_sparse_keycolor"
status=$?
set -e
test "$status" -ne 77 || {
    printf '\n%s\n' "ok 1 - mask sparse keycolor # SKIP threads unavailable"
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' "not ok 1 - mask sparse keycolor"
    exit 0
}
printf '\n%s\n' "ok 1 - mask sparse keycolor"
exit 0
