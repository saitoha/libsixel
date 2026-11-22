#!/bin/sh
# Encode blue in ABGR Oklab while preserving the alpha byte.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" colorspace/0052_abgr_oklab_alpha || {
    echo "not ok 1 - ABGR Oklab conversion preserves alpha and channel order"
    exit 0
}
echo "ok 1 - ABGR Oklab conversion preserves alpha and channel order"
exit 0
