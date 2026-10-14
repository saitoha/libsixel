#!/bin/sh
# Verify explicit target mode rejects PNG without loader fallback.
# Policy: docs/decoder/multi-pass.md

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

${SIXEL_RUNTIME-} "${LSQA_PATH}" -M 6delta-progressive \
    "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" \
    "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" >/dev/null 2>&1 && {
    echo "not ok 1 - non-capture target silently used a loader"
    exit 0
}

echo "ok 1 - multi pass non capture"
exit 0
