#!/bin/sh
# Verify mode7(4ch->CMYK8) multi-layer fallback with valid ICC avoids false
# embedded-ICC failure trace.
# Fixture generation command:
#   python3 tests/data/inputs/formats/generate_psd_policy_trace_fixtures.py

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_LCMS2-}" = 1 || {
    printf "1..0 # SKIP lcms2 support is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/snake16_mode7_cmyk8_missing_composite_multilayer_normal_valid_icc_profile.psd"
trace_log=''
command_status=0

trace_log=$(set +xv; SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin:cms=auto! \
    "${input_psd}" -o /dev/null 2>&1) || command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "mode7 CMYK8 multi-layer valid ICC decode failed: ${trace_log}"
    exit 0
}

case "${trace_log}" in
    *"embedded ICC conversion failed"*)
        echo "not ok" 1 - "mode7 CMYK8 multi-layer valid ICC emitted false failure trace"
        exit 0
        ;;
esac

case "${trace_log}" in
    *"libsixel: loader builtin succeeded"*)
        ;;
    *)
        echo "not ok" 1 - "builtin loader did not finish successfully"
        exit 0
        ;;
esac

echo "ok" 1 - "mode7 CMYK8 multi-layer valid ICC avoids false embedded-ICC failure trace"
exit 0
