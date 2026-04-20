#!/bin/sh
# Validate heckbert rejects invalid profile values.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
msg=''
diag_line=''
status=0
nl='
'

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_DIAG_MODE=code \
    --env SIXEL_DIAG_MODE_QUIET=1 \
    -Q "heckbert:profile=invalid" \
    -p 16 \
    "${input_image}" 2>&1 >/dev/null) || status=$?
test "${status}" -ne 0 || {
    echo "not ok" 1 - "heckbert invalid profile unexpectedly passed"
    exit 0
}
diag_line=${msg%%"${nl}"*}
test "${diag_line#LSXCLI1|phase=option_parse|rc=}" != "${diag_line}" || {
    echo "not ok" 1 - "heckbert invalid profile diagnostic header is malformed"
    exit 0
}
test "${diag_line#*code=UNKNOWN_SUBOPTION_VALUE}" != "${diag_line}" || {
    echo "not ok" 1 - "heckbert invalid profile diagnostic code mismatch"
    exit 0
}
echo "ok" 1 - "heckbert invalid profile rejected"

exit 0
