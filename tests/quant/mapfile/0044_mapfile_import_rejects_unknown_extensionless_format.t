#!/bin/sh
# TAP test: extensionless unknown palette payload is rejected.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
palette_file="${TOP_SRCDIR}/tests/data/inputs/mapfile/unknown-format-noext"

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin \
          -m "${palette_file}" "${snake_png}" \
          -o/dev/null 2>&1) && {
    echo "not ok" 1 - "unknown extensionless palette unexpectedly succeeded"
    exit 0
}

test "${msg#*sixel_prepare_specified_palette: unable to detect palette format.}" != "${msg}" || {
    echo "not ok" 1 - "missing unknown extensionless format diagnostic"
    exit 0
}

echo "ok" 1 - "unknown extensionless palette is rejected"

exit 0
