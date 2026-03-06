#!/bin/sh
# TAP test verifying that img2sixel -H documents every environment variable
# referenced in the sources.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

printf '1..1\n'
set -v
mkdir "${ARTIFACT_LOCAL_DIR}"

"${TOP_SRCDIR}/tests/docs/consistency/list_envvars.sh" --check \
        --img2sixel "${IMG2SIXEL_PATH}" --source-root "${TOP_SRCDIR}" > "${ARTIFACT_LOCAL_DIR}/output.txt" || {
    printf 'not ok 1 - mismatch between sources and -H\n'
    exit 0
}

printf 'ok 1 - environment variables match between sources and -H\n'
exit 0
