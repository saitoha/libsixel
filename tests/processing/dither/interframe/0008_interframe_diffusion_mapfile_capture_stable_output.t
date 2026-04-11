#!/bin/sh
# TAP test ensuring mapfile capture does not change interframe output.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_WEBP-}" = 1 || {
    printf "1..0 # SKIP libwebp loader is unavailable\n"
    exit 0
}

input_webp="${TOP_SRCDIR}/tests/data/inputs/formats/orientation_plain_anim_12x8.webp"
palette_output="${ARTIFACT_LOCAL_DIR}/interframe-mapfile.pal"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --threads=1 \
    -L libwebp \
    -ldisable \
    -S -T 1 \
    -d fs -p 16 \
    "${input_webp}" >/dev/null 2>&1 || {
    printf "1..0 # SKIP animated libwebp frame path is unavailable\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

baseline_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        -L libwebp \
        -ldisable \
        -d interframe -p 16 \
        "${input_webp}"
) || {
    echo "not ok" 1 - "interframe baseline encode failed"
    exit 0
}

captured_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        -L libwebp \
        -ldisable \
        -d interframe -p 16 \
        -M "${palette_output}" \
        "${input_webp}"
) || {
    echo "not ok" 1 - "interframe mapfile encode failed"
    exit 0
}

test -s "${palette_output}" || {
    echo "not ok" 1 - "mapfile output is empty"
    exit 0
}

test "${captured_output}" = "${baseline_output}" || {
    echo "not ok" 1 - "mapfile capture changed interframe output"
    exit 0
}

echo "ok" 1 - "interframe output is stable with mapfile capture"
exit 0
