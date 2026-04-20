#!/bin/sh
# Validate heckbert profile suboption parsing.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..4"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
msg=''
diag_line=''
status=0
nl='
'

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=palette_contract \
    -Q "heckbert:profile=compat" \
    -p 16 \
    "${input_image}" 2>&1 >/dev/null) || status=$?
test "${status}" -eq 0 || {
    echo "not ok" 1 - "heckbert profile=compat encode failed"
    exit 0
}
status=0
diag_line=${msg%%"${nl}"*}
case "${diag_line}" in
    LSXPAL1\|rc=0\|*codes=*) ;;
    *)
        echo "not ok" 1 - "heckbert profile=compat diagnostic header is malformed"
        exit 0
        ;;
esac
case "${diag_line}" in
    *MODEL_HECKBERT*MERGE_AUTO*|*MERGE_AUTO*MODEL_HECKBERT*) ;;
    *)
        echo "not ok" 1 - "heckbert profile=compat contract code mismatch"
        exit 0
        ;;
esac
echo "ok" 1 - "heckbert profile=compat accepted"

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=palette_contract \
    -Q "heckbert:profile=speed" \
    -p 16 \
    "${input_image}" 2>&1 >/dev/null) || status=$?
test "${status}" -eq 0 || {
    echo "not ok" 2 - "heckbert profile=speed encode failed"
    exit 0
}
status=0
diag_line=${msg%%"${nl}"*}
case "${diag_line}" in
    LSXPAL1\|rc=0\|*codes=*) ;;
    *)
        echo "not ok" 2 - "heckbert profile=speed diagnostic header is malformed"
        exit 0
        ;;
esac
case "${diag_line}" in
    *MODEL_HECKBERT*MERGE_NONE*|*MERGE_NONE*MODEL_HECKBERT*) ;;
    *)
        echo "not ok" 2 - "heckbert profile=speed contract code mismatch"
        exit 0
        ;;
esac
echo "ok" 2 - "heckbert profile=speed accepted"

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=palette_contract \
    -Q "heckbert:profile=quality" \
    -p 16 \
    "${input_image}" 2>&1 >/dev/null) || status=$?
test "${status}" -eq 0 || {
    echo "not ok" 3 - "heckbert profile=quality encode failed"
    exit 0
}
status=0
diag_line=${msg%%"${nl}"*}
case "${diag_line}" in
    LSXPAL1\|rc=0\|*codes=*) ;;
    *)
        echo "not ok" 3 - "heckbert profile=quality diagnostic header is malformed"
        exit 0
        ;;
esac
case "${diag_line}" in
    *MODEL_HECKBERT*MERGE_WARD*|*MERGE_WARD*MODEL_HECKBERT*) ;;
    *)
        echo "not ok" 3 - "heckbert profile=quality contract code mismatch"
        exit 0
        ;;
esac
echo "ok" 3 - "heckbert profile=quality accepted"

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_DIAG_MODE=code \
    --env SIXEL_DIAG_MODE_QUIET=1 \
    -Q "heckbert:profile=invalid" \
    -p 16 \
    "${input_image}" 2>&1 >/dev/null) && {
    echo "not ok" 4 - "heckbert invalid profile unexpectedly passed"
    exit 0
}
case "${msg}" in
    LSXCLI1\|phase=option_parse\|rc=*\|code=UNKNOWN_SUBOPTION_VALUE*)
        echo "ok" 4 - "heckbert invalid profile rejected"
        exit 0
        ;;
    *)
        echo "not ok" 4 - "heckbert invalid profile diagnostic code mismatch"
        exit 0
        ;;
esac
