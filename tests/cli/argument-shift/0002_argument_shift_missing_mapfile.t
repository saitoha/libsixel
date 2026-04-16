#!/bin/sh
# TAP test verifying -m requires an argument and does not shift unexpectedly.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}
test "${RUNTIME_ENV_IS_WINE-0}" -eq 1 && {
    printf "1..0 # SKIP wine can intermittently hang in this CLI path\n";
    exit 0
}


echo "1..1"
set -v

# Keep the input tiny because this test only verifies argument parsing.
image_path="${TOP_SRCDIR}/tests/data/inputs/small.ppm"

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -m -w 100 -h 100 "${image_path}" -o/dev/null 2>&1) && {
    echo "not ok" 1 - "accepted -m without argument"
    exit 0
}

trimmed_msg=${msg#*missing required argument for -m,--mapfile option}
test "${trimmed_msg}" != "${msg}" || {
    echo "not ok" 1 - "no diagnostic for missing -m argument"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

echo "ok" 1 - "reports missing mapfile argument"
exit 0
