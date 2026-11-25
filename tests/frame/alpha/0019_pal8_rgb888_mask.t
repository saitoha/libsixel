#!/bin/sh
# RGB888 expansion preserves keyed and masked PAL8 pixels.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical indexed-byte-mask || {
    printf '\n%s\n' 'not ok 1 - RGB888 expansion preserves keyed and masked PAL8 pixels'
    exit 0
}
printf '\n%s\n' 'ok 1 - RGB888 expansion preserves keyed and masked PAL8 pixels'
exit 0
