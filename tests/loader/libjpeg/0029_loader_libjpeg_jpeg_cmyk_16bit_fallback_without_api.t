#!/bin/sh
# Verify fallback to builtin loader for 16-bit CMYK JPEG when libjpeg 16-bit API is unavailable.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_JPEG-}" = 1 || {
    printf "1..0 # SKIP libjpeg loader is unavailable\n"
    exit 0
}

test "${HAVE_JPEG16_API-}" = 1 && {
    printf "1..0 # SKIP libjpeg 16-bit API is available\n"
    exit 0
}

echo "1..1"
set -v

input_jpeg="${TOP_SRCDIR}/tests/data/inputs/formats/snake-jpeg-16bit-cmyk-lossless.jpg"

set +e
trace_log=$(set +xv; SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} \
    "${IMG2SIXEL_PATH}" -L libjpeg,builtin! \
    "${input_jpeg}" -o /dev/null 2>&1)
run_status=$?
set -e

test "${run_status}" -eq 0 || {
    echo "not ok" 1 - "builtin fallback did not decode 16-bit CMYK JPEG"
    exit 0
}

after_libjpeg=${trace_log#*LSXLOAD1|event=fail|loader=libjpeg|code=L_ERR_BAD_INPUT*}
test "${after_libjpeg}" != "${trace_log}" || {
    echo "not ok" 1 - "libjpeg failure code was not reported"
    exit 0
}

test "${after_libjpeg#*LSXLOAD1|event=ok|loader=builtin|code=L_OK*}" \
    != "${after_libjpeg}" || {
    echo "not ok" 1 - "builtin success code was not reported after libjpeg"
    exit 0
}

echo "ok" 1 - "16-bit CMYK input falls back to builtin after libjpeg failure when 16-bit API is unavailable"
exit 0
