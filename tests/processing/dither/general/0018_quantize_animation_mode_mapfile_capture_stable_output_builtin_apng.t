#!/bin/sh
# TAP test ensuring mapfile capture does not change animation_mode output.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

input_apng="${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgba_loop2.png"

test "${ARTIFACT_LOCAL_DIR-}" != "" || {
    printf "1..0 # SKIP ARTIFACT_LOCAL_DIR is not set\n"
    exit 0
}

mkdir -p "${ARTIFACT_LOCAL_DIR}" || {
    printf "1..0 # SKIP cannot create ARTIFACT_LOCAL_DIR\n"
    exit 0
}

palette_output="${ARTIFACT_LOCAL_DIR}/quantize-animation-mode-mapfile.pal"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --threads=1 \
    -L builtin \
    -ldisable \
    -S -T 1 \
    -Qauto -d fs -p 2 \
    "${input_apng}" >/dev/null 2>&1 || {
    printf "1..0 # SKIP animated builtin APNG frame path is unavailable\n"
    exit 0
}

echo "1..1"
set -v

baseline_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        -L builtin \
        -ldisable \
        -Qauto:animation_mode=1 -d fs -p 2 \
        "${input_apng}"
) || {
    echo "not ok" 1 - "animation_mode baseline encode failed"
    exit 0
}

mapfile_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        -L builtin \
        -ldisable \
        -Qauto:animation_mode=1 -d fs -p 2 \
        -M "${palette_output}" \
        "${input_apng}"
) || {
    echo "not ok" 1 - "animation_mode mapfile capture encode failed"
    exit 0
}

test "${baseline_output}" = "${mapfile_output}" || {
    echo "not ok" 1 - "mapfile capture changed animation_mode output"
    exit 0
}

echo "ok" 1 - "mapfile capture keeps animation_mode output stable"
exit 0
