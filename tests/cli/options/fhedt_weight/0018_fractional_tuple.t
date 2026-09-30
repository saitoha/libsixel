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
trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 -Woklab -~fhedt:weights=4.5/1/.75 "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" 2>&1 >/dev/null) || {
    echo "not ok 1 - FHEDT conversion failed"
    exit 0
}
case "${trace}" in
    *LSXLUT1\|policy=fhedt\|precision=float32\|*weights=4.5/1/0.75*) true ;;
    *) false ;;
esac || {
    echo "not ok 1 - fractional tuple"
    exit 0
}

echo "ok 1 - fractional tuple"
exit 0
