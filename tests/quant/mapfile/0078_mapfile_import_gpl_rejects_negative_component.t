#!/bin/sh
# TAP test: GPL import rejects negative RGB components.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
gpl_palette="${TOP_SRCDIR}/tests/data/inputs/mapfile/gpl-negative-component-invalid.gpl"

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin \
          -m gpl:"${gpl_palette}" "${snake_png}" \
          -o/dev/null 2>&1) && {
    echo "not ok" 1 - "GPL negative-component payload unexpectedly succeeded"
    exit 0
}

test "${msg#*sixel_palette_parse_gpl: invalid component.}" != "${msg}" || {
    echo "not ok" 1 - "missing GPL negative-component diagnostic"
    exit 0
}

echo "ok" 1 - "GPL negative-component payload is rejected"

exit 0
