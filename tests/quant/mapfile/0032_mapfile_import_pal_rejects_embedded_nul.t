#!/bin/sh
# TAP test: JASC-PAL import rejects embedded NUL bytes.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
pal_file="${TOP_SRCDIR}/tests/data/inputs/mapfile/pal-embedded-nul-invalid.pal"

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
          -m pal-jasc:"${pal_file}" "${snake_png}" \
          -o/dev/null 2>&1) && {
    echo "not ok" 1 - "JASC-PAL with embedded NUL unexpectedly succeeded"
    exit 0
}

test "${msg#*sixel_palette_parse_pal_jasc: embedded NUL byte.}" != "${msg}" || {
    echo "not ok" 1 - "missing JASC embedded-NUL diagnostic"
    exit 0
}

echo "ok" 1 - "JASC-PAL with embedded NUL is rejected"

exit 0
