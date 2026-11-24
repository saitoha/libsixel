#!/bin/sh
# Packed G2 normalization rejects a zero-width row.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/format/0068_normalize_g2_empty_width || {
    printf '\n%s\n' "not ok 1 - Packed G2 normalization rejects a zero-width row"
    exit 0
}
printf '\n%s\n' "ok 1 - Packed G2 normalization rejects a zero-width row"
exit 0
