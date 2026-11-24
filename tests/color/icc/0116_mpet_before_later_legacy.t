#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0116_mpet_before_later_legacy || {
    printf '\n%s\n' "not ok 1 - perceptual MPE precedes the saturation legacy pair"
    exit 0
}

printf '\n%s\n' "ok 1 - perceptual MPE precedes the saturation legacy pair"
exit 0
