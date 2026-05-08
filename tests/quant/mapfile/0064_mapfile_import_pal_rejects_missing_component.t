#!/bin/sh
# TAP test: JASC-PAL import rejects missing RGB components.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
pal_palette="${TOP_SRCDIR}/tests/data/inputs/mapfile/pal-missing-component-invalid.pal"

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin \
          -m pal-jasc:"${pal_palette}" "${snake_png}" \
          -o/dev/null 2>&1) && {
    echo "not ok" 1 - "PAL missing-component payload unexpectedly succeeded"
    exit 0
}

test "${msg#*sixel_palette_parse_pal_jasc: invalid component.}" != "${msg}" || {
    echo "not ok" 1 - "missing PAL missing-component diagnostic"
    exit 0
}

echo "ok" 1 - "PAL missing-component payload is rejected"

exit 0
