#!/bin/sh
# Policy: docs/encoder/palette-construction/cover-policy/README.md
# AUTO growth preserves the reserved slot and 255 entries.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/auto_cover_float/0004_reserved_capacity || {
    printf '\n%s\n' "not ok 1 - AUTO growth preserves the reserved slot and 255 entries"
    exit 0
}
printf '\n%s\n' "ok 1 - AUTO growth preserves the reserved slot and 255 entries"
exit 0
