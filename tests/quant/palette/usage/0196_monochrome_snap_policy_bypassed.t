#!/bin/sh
# Policy: docs/encoder/palette-construction/snap-policy/README.md
# Verify monochrome accepts but bypasses snap policy.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"

trace=$(set +xv; SIXEL_TRACE_TOPIC=palette_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threading-policy=1 \
    -e --snap-policy=reversible -o/dev/null "${input_image}" \
    2>&1 >/dev/null) || {
    echo "not ok" 1 - "monochrome conversion with snap policy failed"
    exit 0
}
case "${trace}" in *LSXSNP1\|**) false ;; *) true ;; esac || {
    echo "not ok" 1 - "snap policy unexpectedly reached monochrome palette"
    exit 0
}

echo "ok" 1 - "monochrome bypasses snap policy"
exit 0
