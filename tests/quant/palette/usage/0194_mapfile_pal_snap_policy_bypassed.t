#!/bin/sh
# Policy: docs/functionality/snap-policy.md
# Verify a PAL mapfile accepts but bypasses snap policy.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"

trace=$(set +xv; SIXEL_TRACE_TOPIC=palette_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 \
    -m pal:- --snap-policy=reversible -o/dev/null "${input_image}" \
    2>&1 >/dev/null <<'PAL'
JASC-PAL
0100
8
162 6 6
6 178 6
150 158 6
126 106 250
194 6 182
6 174 186
194 194 194
2 2 2
PAL
) || {
    echo "not ok" 1 - "PAL mapfile conversion with snap policy failed"
    exit 0
}
test "${trace#*LSXSNP1|*}" = "${trace}" || {
    echo "not ok" 1 - "snap policy unexpectedly reached PAL mapfile palette"
    exit 0
}

echo "ok" 1 - "PAL mapfile bypasses snap policy"
exit 0
