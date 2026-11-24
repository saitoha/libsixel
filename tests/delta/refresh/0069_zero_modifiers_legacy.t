#!/bin/sh
# Verify zero modifiers legacy.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    delta/refresh/0069_zero_modifiers_legacy || status=$?
test "$status" -eq 0 || {
    printf '\n%s\n' "not ok 1 - zero modifiers legacy"
    exit 0
}
printf '\n%s\n' "ok 1 - zero modifiers legacy"
exit 0
