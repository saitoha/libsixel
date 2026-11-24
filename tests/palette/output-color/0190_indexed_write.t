#!/bin/sh
# Indexed serialization reconciles byte edits when application is bypassed
# Policy: docs/encoder/output/output-colorspace.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/retention_indexed_write" || {
    printf '\n%s\n' "not ok 1 - Indexed serialization reconciles byte edits when application is bypassed"
    exit 0
}
printf '\n%s\n' "ok 1 - Indexed serialization reconciles byte edits when application is bypassed"
exit 0
