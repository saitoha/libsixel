#!/bin/sh
# Mutable byte edits affect lookup before serialization
# Policy: docs/encoder/output/output-colorspace.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/retention_getter_write" || {
    printf '\n%s\n' "not ok 1 - Mutable byte edits affect lookup before serialization"
    exit 0
}
printf '\n%s\n' "ok 1 - Mutable byte edits affect lookup before serialization"
exit 0
