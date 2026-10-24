#!/bin/sh
# Policy: docs/loader/libpng.md
# Verify actual CLI clear-header and source-derived paint coverage.
set -eux
test "${HAVE_LIBPNG-}" = 1 || {
    echo "1..0 # SKIP libpng support is disabled"
    exit 0
}
test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled"
    exit 0
}
echo "1..1"
set -v

output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --threading-policy=greedy:global_budget=1 -Goff -dnone -Llibpng:K1:cms_engine=none! \
    "${TOP_SRCDIR}/images/pngsuite/transparency/tbrn2c08.png") || {
    echo "not ok 1 - keycolor CLI conversion"
    exit 0
}
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" png-keycolor/0002_paint_mask \
    "${output}" || {
    echo "not ok 1 - keycolor paint coverage"
    exit 0
}
echo "ok 1 - keycolor paint coverage"
exit 0
