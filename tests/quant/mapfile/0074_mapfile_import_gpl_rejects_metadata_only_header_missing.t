#!/bin/sh
# TAP test: GPL import rejects metadata-only payloads without a header.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
gpl_palette="${TOP_SRCDIR}/tests/data/inputs/mapfile/gpl-metadata-only-invalid.gpl"

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin \
          -m gpl:"${gpl_palette}" "${snake_png}" \
          -o/dev/null 2>&1) && {
    echo "not ok" 1 - "GPL metadata-only payload unexpectedly succeeded"
    exit 0
}

test "${msg#*sixel_palette_parse_gpl: header missing.}" != "${msg}" || {
    echo "not ok" 1 - "missing GPL header-missing diagnostic"
    exit 0
}

echo "ok" 1 - "GPL metadata-only payload is rejected"

exit 0
