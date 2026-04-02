#!/bin/sh
# TAP test: extensionless ACT-sized data is rejected as ambiguous.

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

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin \
          -m "${act_file}" "${snake_png}" \
          -o/dev/null 2>&1) && {
    echo "not ok" 1 - "extensionless ACT-sized input unexpectedly succeeded"
    exit 0
}

test "${msg#*sixel_prepare_specified_palette: ambiguous ACT payload without extension; use act:PATH.}" != "${msg}" || {
    echo "not ok" 1 - "missing extensionless ACT ambiguity diagnostic"
    exit 0
}

echo "ok" 1 - "extensionless ACT-sized input is rejected"

exit 0
