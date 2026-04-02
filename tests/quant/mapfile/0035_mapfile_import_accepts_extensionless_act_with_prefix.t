#!/bin/sh
# TAP test: extensionless ACT-sized data works with an explicit act: prefix.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
act_file="${TOP_SRCDIR}/tests/data/inputs/mapfile/act-size-only-noext"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -m act:"${act_file}" "${snake_png}" -o/dev/null || {
    echo "not ok" 1 - "explicit act: prefix failed for extensionless ACT-sized input"
    exit 0
}

echo "ok" 1 - "explicit act: prefix accepts extensionless ACT-sized input"

exit 0
