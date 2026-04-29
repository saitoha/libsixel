#!/bin/sh
# TAP test for loader fallback when libjpeg 12-bit API is unavailable.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_JPEG-}" = 1 || {
    printf "1..0 # SKIP libjpeg loader is unavailable\n"
    exit 0
}

test "${HAVE_JPEG12_API-}" = 1 && {
    printf "1..0 # SKIP libjpeg 12-bit API is available\n"
    exit 0
}

echo "1..1"
set -v

input_jpeg="${TOP_SRCDIR}/tests/data/inputs/formats/snake-jpeg-12bit.jpg"

trace_log=$(set +xv; SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} \
    "${IMG2SIXEL_PATH}" -L libjpeg,builtin! \
    "${input_jpeg}" -o /dev/null 2>&1 || true)

after_libjpeg=${trace_log#*LSXLOAD1|event=fail|loader=libjpeg|code=L_ERR_BAD_INPUT*}
test "${after_libjpeg}" != "${trace_log}" || {
    echo "not ok" 1 - "libjpeg failure code was not reported"
    exit 0
}

test "${after_libjpeg#*LSXLOAD1|event=try|loader=builtin|code=L_TRY*}" \
    != "${after_libjpeg}" || {
    echo "not ok" 1 - "builtin loader was not attempted after libjpeg"
    exit 0
}

echo "ok" 1 - "12-bit input falls back after libjpeg failure when API is unavailable"
exit 0
