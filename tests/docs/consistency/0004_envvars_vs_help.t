#!/bin/sh
# TAP test verifying that img2sixel -H documents every environment variable
# referenced in the sources.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

printf '1..1\n'
set -v

"${top_srcdir}/tests/docs/consistency/list_envvars.sh" --check \
        --img2sixel "${IMG2SIXEL_PATH}" --source-root "${top_srcdir}" > "${ARTIFACT_LOCAL_DIR}/output.txt" || {
    printf 'not ok 1 - mismatch between sources and -H\n'
    exit 0
}

printf 'ok 1 - environment variables match between sources and -H\n'
exit 0
