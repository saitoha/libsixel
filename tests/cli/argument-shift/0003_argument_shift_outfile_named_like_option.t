#!/bin/sh
# TAP test verifying output file named like an option is accepted.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

image_path="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"
outfile_err="${ARTIFACT_LOCAL_DIR}/outfile-option-name.err"
rm -f "${outfile_err}" "${ARTIFACT_LOCAL_DIR}/-p"

cd "${ARTIFACT_LOCAL_DIR}" && {
    if ! run_img2sixel -o -p "${image_path}" >/dev/null 2>"${outfile_err}"; then
        printf 'not ok 1 %s\n' "outfile named like option rejected"
        printf '%s\n' '--- stderr ---' >&2
        cat "${outfile_err}" >&2 2>/dev/null || :
        exit 0
    fi
}

if test ! -s "${ARTIFACT_LOCAL_DIR}/-p"; then
    printf 'not ok 1 %s\n' "outfile named like option missing"
    exit 0
fi

printf 'ok 1 %s\n' "outfile named like option is supported"

exit 0
