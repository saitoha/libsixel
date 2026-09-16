#!/bin/sh
# Verify lookup policy does not change PAL mapfile palette export.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
expected_palette='JASC-PAL
0100
8
162 6 6
6 178 6
150 158 6
126 106 250
194 6 182
6 174 186
194 194 194
2 2 2'

actual_none=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        -m pal:- -~ none -M pal:- -o /dev/null "${input_image}" <<'PAL'
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
    echo "not ok" 1 - "PAL mapfile export failed with -~ none"
    exit 0
}
actual_none=$(printf "%s" "${actual_none}" | tr -d '\015') || {
    echo "not ok" 1 - "PAL none mapfile output normalization failed"
    exit 0
}

actual_eytzinger=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        -m pal:- -~ eytzinger -M pal:- -o /dev/null "${input_image}" <<'PAL'
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
    echo "not ok" 1 - "PAL mapfile export failed with -~ eytzinger"
    exit 0
}
actual_eytzinger=$(printf "%s" "${actual_eytzinger}" | tr -d '\015') || {
    echo "not ok" 1 - "PAL eytzinger mapfile output normalization failed"
    exit 0
}

actual_fhedt=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        -m pal:- -~ fhedt -M pal:- -o /dev/null "${input_image}" <<'PAL'
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
    echo "not ok" 1 - "PAL mapfile export failed with -~ fhedt"
    exit 0
}
actual_fhedt=$(printf "%s" "${actual_fhedt}" | tr -d '\015') || {
    echo "not ok" 1 - "PAL fhedt mapfile output normalization failed"
    exit 0
}

test "${actual_none}" = "${expected_palette}" || {
    echo "not ok" 1 - "PAL mapfile changed under -~ none"
    exit 0
}
test "${actual_eytzinger}" = "${expected_palette}" || {
    echo "not ok" 1 - "PAL mapfile changed under -~ eytzinger"
    exit 0
}
test "${actual_fhedt}" = "${expected_palette}" || {
    echo "not ok" 1 - "PAL mapfile changed under -~ fhedt"
    exit 0
}

echo "ok" 1 - "PAL mapfile export is stable under lookup policies"
exit 0
