#!/bin/sh
# TAP test verifying output file named like an option is accepted.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

image_path="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"
out_file="${ARTIFACT_LOCAL_DIR}/-p"

: >"${out_file}"

cd "${ARTIFACT_LOCAL_DIR}" && {
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -o -p "${image_path}" >/dev/null || {
        echo "not ok" 1 - "outfile named like option rejected"
        printf '%s\n' '--- stderr ---' >&2
        exit 0
    }
}

cd "${TOP_SRCDIR}" || {
    echo "not ok" 1 - "failed to return to source directory"
    exit 0
}

test -s "${out_file}" || {
    echo "not ok" 1 - "outfile named like option missing"
    exit 0
}

echo "ok" 1 - "outfile named like option is supported"
exit 0
