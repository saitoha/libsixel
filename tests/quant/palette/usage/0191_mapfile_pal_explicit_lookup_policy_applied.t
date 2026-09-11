#!/bin/sh
# Verify explicit FHEDT reaches palette application with a PAL mapfile.
# Policy: docs/functionality/external-palettes.md
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"

trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 -Goff -dnone \
    -m pal:- --lookup-policy=fhedt -o/dev/null "${input_image}" \
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
    echo "not ok" 1 - "PAL mapfile conversion with FHEDT failed"
    exit 0
}
test "${trace#*LSXLUT2|phase=palette-apply|selected=lookup/fhedt.8bit|explicit=1*}" \
    != "${trace}" || {
    echo "not ok" 1 - "explicit FHEDT missed PAL mapfile palette application"
    exit 0
}

echo "ok" 1 - "explicit FHEDT applies to a PAL mapfile"
exit 0
