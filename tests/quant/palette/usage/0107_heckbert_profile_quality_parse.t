#!/bin/sh
# Validate heckbert profile=quality parsing.
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
    --env SIXEL_TRACE_TOPIC=palette_contract \
    -Q "heckbert:profile=quality" \
    -p 16 \
    "${input_image}" 2>&1 >/dev/null) || status=$?
test "${status}" -eq 0 || {
    echo "not ok" 1 - "heckbert profile=quality encode failed"
    exit 0
}
diag_line=${msg%%"${nl}"*}
test "${diag_line#LSXPAL1|rc=0|}" != "${diag_line}" || {
    echo "not ok" 1 - "heckbert profile=quality diagnostic header is malformed"
    exit 0
}
test "${diag_line#*codes=}" != "${diag_line}" || {
    echo "not ok" 1 - "heckbert profile=quality diagnostic header is malformed"
    exit 0
}
test "${diag_line#*MODEL_HECKBERT}" != "${diag_line}" || {
    echo "not ok" 1 - "heckbert profile=quality contract code mismatch"
    exit 0
}
test "${diag_line#*MERGE_WARD}" != "${diag_line}" || {
    echo "not ok" 1 - "heckbert profile=quality contract code mismatch"
    exit 0
}
echo "ok" 1 - "heckbert profile=quality accepted"

exit 0
