#!/bin/sh
# Policy: docs/decoder/multi-pass.md
# The capture and independent reference both retain five SSIM scales.
set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    echo "1..0 # SKIP sixel2png is disabled"
    exit 0
}
test -n "${LSQA_PATH-}" && test -x "${LSQA_PATH}" || {
    echo "1..0 # SKIP lsqa is disabled"
    exit 0
}

echo "1..1"
set -v

full_score=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -M6delta-progressive \
    -m MS-SSIM -b MS-SSIM:0.98 \
    "${TOP_SRCDIR}/tests/data/inputs/snake_192.png" \
    "${TOP_SRCDIR}/tests/data/inputs/progressive-snake-192.six") || {
    echo "not ok 1 - -M6delta-progressive did not assess the final canvas"
    exit 0
}

short_score=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -M6 \
    -m MS-SSIM -b MS-SSIM:0.98 \
    "${TOP_SRCDIR}/tests/data/inputs/snake_192.png" \
    "${TOP_SRCDIR}/tests/data/inputs/progressive-snake-192.six") || {
    echo "not ok 1 - -M6 did not assess the final canvas"
    exit 0
}

long_score=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" --input-mode=6delta-pro \
    -m MS-SSIM -b MS-SSIM:0.98 \
    "${TOP_SRCDIR}/tests/data/inputs/snake_192.png" \
    "${TOP_SRCDIR}/tests/data/inputs/progressive-snake-192.six") || {
    echo "not ok 1 - --input-mode=6delta-pro did not assess the final canvas"
    exit 0
}

test "${short_score}" = "${full_score}" && \
    test "${long_score}" = "${full_score}" || {
    echo "not ok 1 - input mode prefix scores differ from the full name"
    exit 0
}

# The other unique prefix must preserve ordinary image loading.
single_score=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -Msingle -m MS-SSIM \
    "${TOP_SRCDIR}/tests/data/inputs/snake_192.png" \
    "${TOP_SRCDIR}/tests/data/inputs/snake_192.png") || {
    echo "not ok 1 - single mode assessment failed"
    exit 0
}
single_prefix_score=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -Ms -m MS-SSIM \
    "${TOP_SRCDIR}/tests/data/inputs/snake_192.png" \
    "${TOP_SRCDIR}/tests/data/inputs/snake_192.png") || {
    echo "not ok 1 - single prefix assessment failed"
    exit 0
}
test "${single_prefix_score}" = "${single_score}" || {
    echo "not ok 1 - single prefix lost ordinary image loading"
    exit 0
}
echo "ok 1 - input mode prefixes assess the same final canvas"
exit 0
