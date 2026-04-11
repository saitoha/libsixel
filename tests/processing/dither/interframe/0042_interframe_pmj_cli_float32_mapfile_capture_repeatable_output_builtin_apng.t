#!/bin/sh
# TAP test ensuring float32 PMJ CLI mapfile capture output is repeatable.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

input_apng="${TOP_SRCDIR}/tests/data/inputs/formats/orientation_plain_apng_12x8_rgba_loop2.png"
palette_output="${ARTIFACT_LOCAL_DIR}/interframe-float32-pmj-cli-mapfile-builtin-apng.pal"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --threads=1 \
    --precision=float32 \
    -L builtin \
    -ldisable \
    -S -T 1 \
    -d fs -Y direct -p 16 \
    "${input_apng}" >/dev/null 2>&1 || {
    printf "1..0 # SKIP animated builtin APNG frame path is unavailable\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

first_capture_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        --precision=float32 \
        -L builtin \
        -ldisable \
        -d stbn:source=pmj -p 16 \
        -M "${palette_output}" \
        "${input_apng}"
) || {
    echo "not ok" 1 - "float32 pmj cli first mapfile encode failed"
    exit 0
}

second_capture_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        --precision=float32 \
        -L builtin \
        -ldisable \
        -d stbn:source=pmj -p 16 \
        -M "${palette_output}" \
        "${input_apng}"
) || {
    echo "not ok" 1 - "float32 pmj cli second mapfile encode failed"
    exit 0
}

test -s "${palette_output}" || {
    echo "not ok" 1 - "float32 pmj cli mapfile output is empty"
    exit 0
}

test "${second_capture_output}" = "${first_capture_output}" || {
    echo "not ok" 1 - "float32 pmj cli mapfile output is not repeatable"
    exit 0
}

echo "ok" 1 - "float32 pmj cli mapfile output is repeatable"
exit 0
