#!/bin/sh
# Verify ACT transparency metadata does not offset imported palette entries.
# Policy: docs/functionality/external-palettes.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
act_palette="${ARTIFACT_LOCAL_DIR}/palette.act"
expected_palette='JASC-PAL
0100
2
0 0 0
255 255 255'

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! --threads=1 \
    -Goff -dnone --lookup-policy=none -bgray1 -M "${act_palette}" \
    -o /dev/null "${input_image}" || {
    echo "not ok" 1 - "failed to create ACT regression palette"
    exit 0
}

printf '\000\377' | dd of="${act_palette}" bs=1 seek=770 conv=notrunc \
    2>/dev/null || {
    echo "not ok" 1 - "failed to set ACT transparency index"
    exit 0
}

actual_palette=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! --threads=1 \
        -Goff -dnone --lookup-policy=none -m "${act_palette}" \
        -M pal:- -o /dev/null "${input_image}"
) || {
    echo "not ok" 1 - "ACT palette with transparency index was rejected"
    exit 0
}
actual_palette=$(printf "%s" "${actual_palette}" | tr -d '\015') || {
    echo "not ok" 1 - "ACT palette output normalization failed"
    exit 0
}

test "${actual_palette}" = "${expected_palette}" || {
    echo "not ok" 1 - "ACT transparency index offset palette entries"
    exit 0
}

echo "ok" 1 - "ACT transparency metadata does not offset palette entries"

exit 0
