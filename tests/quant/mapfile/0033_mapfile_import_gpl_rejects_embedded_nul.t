#!/bin/sh
# TAP test: GPL import rejects embedded NUL bytes.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
gpl_file="${TOP_SRCDIR}/tests/data/inputs/mapfile/gpl-embedded-nul-invalid.gpl"

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
          -m gpl:"${gpl_file}" "${snake_png}" \
          -o/dev/null 2>&1) && {
    echo "not ok" 1 - "GPL with embedded NUL unexpectedly succeeded"
    exit 0
}

test "${msg#*sixel_palette_parse_gpl: embedded NUL byte.}" != "${msg}" || {
    echo "not ok" 1 - "missing GPL embedded-NUL diagnostic"
    exit 0
}

echo "ok" 1 - "GPL with embedded NUL is rejected"

exit 0
