#!/bin/sh
# TAP test ensuring mapfile capture is stable on builtin APNG path.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

input_apng="${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgba_loop2.png"
palette_output="${ARTIFACT_LOCAL_DIR}/interframe-mapfile-builtin-apng.pal"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --threads=1 \
    -L builtin \
    -ldisable \
    -S -T 1 \
    -d fs -p 2 \
    "${input_apng}" >/dev/null 2>&1 || {
    printf "1..0 # SKIP animated builtin APNG frame path is unavailable\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

baseline_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        -L builtin \
        -ldisable \
        -S -T 1 \
        -d interframe -p 2 \
        "${input_apng}"
) || {
    echo "not ok" 1 - "interframe builtin APNG baseline encode failed"
    exit 0
}

captured_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        -L builtin \
        -ldisable \
        -S -T 1 \
        -d interframe -p 2 \
        -M "${palette_output}" \
        "${input_apng}"
) || {
    echo "not ok" 1 - "interframe builtin APNG mapfile encode failed"
    exit 0
}

test -s "${palette_output}" || {
    echo "not ok" 1 - "builtin APNG mapfile output is empty"
    exit 0
}

test "${captured_output}" = "${baseline_output}" || {
    echo "not ok" 1 - "builtin APNG mapfile capture changed interframe output"
    exit 0
}

echo "ok" 1 - "interframe output is stable with mapfile on builtin APNG path"
exit 0
