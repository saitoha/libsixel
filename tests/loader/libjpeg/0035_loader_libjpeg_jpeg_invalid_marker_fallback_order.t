#!/bin/sh
# Verify runtime decode failure in libjpeg falls through to builtin loader.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_JPEG-}" = 1 || {
    printf "1..0 # SKIP libjpeg loader is unavailable\n"
    exit 0
}

echo "1..1"
set -v

input_jpeg="${TOP_SRCDIR}/tests/data/corrupted/invalid_marker.jpg"

set +e
trace_log=$(set +xv; SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} \
    "${IMG2SIXEL_PATH}" -L libjpeg,builtin! \
    "${input_jpeg}" -o /dev/null 2>&1)
run_status=$?
set -e

test "${run_status}" -ne 0 || {
    echo "not ok" 1 - "invalid-marker JPEG unexpectedly decoded"
    exit 0
}

after_libjpeg=${trace_log#*LSXLOAD1|event=fail|loader=libjpeg|code=L_ERR_BAD_INPUT*}
test "${after_libjpeg}" != "${trace_log}" || {
    echo "not ok" 1 - "libjpeg failure code was not reported"
    exit 0
}

after_builtin=${after_libjpeg#*LSXLOAD1|event=try|loader=builtin|code=L_TRY*}
test "${after_builtin}" != "${after_libjpeg}" || {
    echo "not ok" 1 - "builtin loader was not attempted after libjpeg"
    exit 0
}

test "${after_builtin#*LSXLOAD1|event=fail|loader=builtin|code=*}" \
    != "${after_builtin}" || {
    echo "not ok" 1 - "builtin failure code was not reported after libjpeg"
    exit 0
}

echo "ok" 1 - "invalid-marker JPEG follows libjpeg-to-builtin fallback order"
exit 0
