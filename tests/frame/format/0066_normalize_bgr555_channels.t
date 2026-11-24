#!/bin/sh
# BGR555 normalization preserves the channel order.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/format/0066_normalize_bgr555_channels || {
    printf '\n%s\n' "not ok 1 - BGR555 normalization preserves the channel order"
    exit 0
}
printf '\n%s\n' "ok 1 - BGR555 normalization preserves the channel order"
exit 0
