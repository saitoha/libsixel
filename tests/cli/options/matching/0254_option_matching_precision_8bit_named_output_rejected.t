#!/bin/sh
# Verify precision-policy=8bit rejects a named -U destination.
# Policy: docs/encoder/precision/README.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
status=0
log=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threading-policy=1 \
    --precision-policy=8bit -Udisplay-p3 -p16 \
    -o/dev/null "${input_image}" 2>&1) || status=$?
test "${status}" -ne 0 || {
    echo "not ok 1 - 8bit with a Display P3 destination was accepted"
    exit 0
}
case "${log}" in
    *'precision-policy=8bit cannot be combined with a named -U destination'*)
        true ;;
    *) false ;;
esac || {
    echo "not ok 1 - rejection did not name the -U destination"
    printf '# %s\n' "${log}"
    exit 0
}

echo "ok 1 - 8bit rejects a named -U destination"
exit 0
