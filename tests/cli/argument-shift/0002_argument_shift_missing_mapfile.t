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
msg=''
status=0

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_DIAG_MODE=code \
    --env SIXEL_DIAG_MODE_QUIET=1 \
    -m -w 100 -h 100 "${image_path}" -o/dev/null \
    2>&1 >/dev/null) || status=$?

test "${status}" -eq 2 || {
    echo "not ok" 1 - "accepted -m without argument"
    exit 0
}

trimmed_msg=${msg#*missing required argument for -m,--mapfile option}
test "${trimmed_msg}" != "${msg}" || {
    echo "not ok" 1 - "no diagnostic for missing -m argument"
    exit 0
}

echo "ok" 1 - "reports missing mapfile argument"
exit 0
