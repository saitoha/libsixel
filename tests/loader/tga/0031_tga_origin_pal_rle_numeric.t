#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/tga.md
# Rebase a repeated PAL8 index once when reading its RLE packet.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0231_tga_origin_pal_rle_numeric" 1>&2 || {
    printf '\n%s\n' "not ok 1 - Rebase a repeated PAL8 index once when reading its RLE packet"
    exit 0
}
printf '\n%s\n' "ok 1 - Rebase a repeated PAL8 index once when reading its RLE packet"
exit 0
