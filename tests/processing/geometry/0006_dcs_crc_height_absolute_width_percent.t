#!/bin/sh
# Check DCS coordinates for geometry scaling combinations.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

expected="3871514854 39"
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

source_sixel="${ARTIFACT_LOCAL_DIR}/dcs-height-absolute-input.sixel"
scaled_sixel="${ARTIFACT_LOCAL_DIR}/dcs-height-absolute-output.sixel"

printf '%b' '\033Pq"1;1;1;1!6~\033\057' >"${source_sixel}" || {
    echo "not ok" 1 - "DCS coordinates stayed consistent (input write failed)"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -=1 -rne -h12 -w200% \
    <"${source_sixel}" >"${scaled_sixel}" || {
    echo "not ok" 1 - "DCS coordinates stayed consistent"
    exit 0
}

test -s "${scaled_sixel}" || {
    echo "not ok" 1 - "DCS coordinates stayed consistent (no payload produced)"
    exit 0
}

# GNV exposes an OpenVMS text-record terminator as a trailing newline.
sum=$(tr -d '\n' <"${scaled_sixel}" | cksum) || {
    echo "not ok" 1 - "DCS coordinates stayed consistent (cksum failed)"
    exit 0
}

test "${sum}" = "${expected}" || {
    echo "not ok" 1 - "DCS coordinates stayed consistent"
    exit 0
}

echo "ok" 1 - "DCS coordinates stayed consistent"
exit 0
