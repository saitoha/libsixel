#!/bin/sh
# TAP test: img2sixel fetches local files via the file scheme.

set -eux

test "${HAVE_LIBCURL-}" = 1 || test "${HAVE_LIBFETCH-}" = 1 || {
    printf "1..0 # SKIP libcurl/libfetch support is disabled in this build\n"
    exit 0
}
test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

local_file="file://${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" "${local_file}" >"${ARTIFACT_LOCAL_DIR}/local-file.sixel" || {
    echo "not ok" 1 - "local file fetch via file scheme failed"
    exit 0
}

echo "ok" 1 - "fetches local file via file scheme"
exit 0
