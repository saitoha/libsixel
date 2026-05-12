#!/bin/sh
# Verify RGB JPEG with non-applicable CMYK ICC keeps silent skip behavior.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_jpeg="${TOP_SRCDIR}/tests/data/inputs/formats/snake-64-embedded-cmykprofile.jpg"

trace_log=''
command_status=0

trace_log=$(set +xv; SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin:cms_engine=auto! \
    "${input_jpeg}" -o /dev/null 2>&1) || command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "RGB JPEG decode failed unexpectedly: ${trace_log}"
    exit 0
}

test "${trace_log#*builtin JPEG: embedded ICC conversion failed*}" = \
    "${trace_log}" || {
    echo "not ok" 1 - "non-applicable CMYK ICC was misclassified as conversion failure"
    exit 0
}

echo "ok" 1 - "RGB JPEG with non-applicable CMYK ICC preserves skip trace contract"
exit 0
