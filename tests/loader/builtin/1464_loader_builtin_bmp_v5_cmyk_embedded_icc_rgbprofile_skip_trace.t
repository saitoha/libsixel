#!/bin/sh
# Verify CMYK BMP with non-applicable RGB ICC keeps silent skip behavior.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_bmp="${TOP_SRCDIR}/tests/data/inputs/formats/bmp-v5-cmyk-embedded-icc-rgbprofile-2x2.bmp"

trace_log=''
command_status=0

trace_log=$(set +xv; SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin:cms=auto! \
    "${input_bmp}" -o /dev/null 2>&1) || command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "CMYK BMP decode failed unexpectedly: ${trace_log}"
    exit 0
}

test "${trace_log#*builtin BMP: CMYK ICC conversion failed*}" = \
    "${trace_log}" || {
    echo "not ok" 1 - "non-applicable RGB ICC was misclassified as CMYK conversion failure"
    exit 0
}

test "${trace_log#*builtin BMP: invalid embedded ICC size*}" = \
    "${trace_log}" || {
    echo "not ok" 1 - "non-applicable RGB ICC was misclassified as malformed ICC size"
    exit 0
}

test "${trace_log#*builtin BMP: embedded ICC range is invalid*}" = \
    "${trace_log}" || {
    echo "not ok" 1 - "non-applicable RGB ICC was misclassified as malformed ICC range"
    exit 0
}

echo "ok" 1 - "CMYK BMP with non-applicable RGB ICC preserves skip trace contract"
exit 0
