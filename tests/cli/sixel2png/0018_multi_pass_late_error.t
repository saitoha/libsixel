#!/bin/sh
# Verify malformed final input produces no prefix PNG.
# Policy: docs/decoder/multi-pass.md
# Policy: docs/misc/platforms/openvms.md
# Coverage: OV-13

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    echo "1..0 # SKIP sixel2png is disabled"
    exit 0
}

echo "1..1"
set -v

# A regular file preserves the incomplete suffix and provides reliable EOF
# on GNV, where a short producer pipeline can retain its writer endpoint.
output=$(${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -M 6delta-progressive \
    -i "${TOP_SRCDIR}/tests/data/inputs/multi-pass-incomplete-suffix.six" \
    2>/dev/null) && {
    echo "not ok 1 - incomplete suffix accepted"
    exit 0
}
test -z "${output}" || {
    echo "not ok 1 - prefix PNG published before EOF validation"
    exit 0
}

echo "ok 1 - multi pass late error"
exit 0
