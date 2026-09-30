#!/bin/sh
# Verify FHEDT weights setting precedence and scope.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}
echo "1..1"
set -v

unset SIXEL_LOOKUP_FHEDT_WEIGHTS
set +e
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 -Wc -~fhedt:weights=1//1 "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" >/dev/null 2>&1
status=$?
set -e
test "${status}" -ne 0 || {
    echo "not ok 1 - reject empty axis"
    exit 0
}

echo "ok 1 - reject empty axis"
exit 0
