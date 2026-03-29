#!/bin/sh
# Verify native CMYK8 multi-layer fallback keeps embedded-ICC conversion
# failure trace behavior for bad ICC bytes.
# Fixture generation command:
#   python3 tests/data/inputs/formats/generate_psd_policy_trace_fixtures.py

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/snake16_cmyk8_missing_composite_multilayer_normal_bad_icc_profile.psd"
trace_log=''
command_status=0

trace_log=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -v -Lbuiltin:cms=auto! \
    "${input_psd}" -o /dev/null 2>&1) || command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "native CMYK8 multi-layer bad ICC decode failed: ${trace_log}"
    exit 0
}

case "${trace_log}" in
    *"embedded ICC conversion failed"*)
        ;;
    *)
        echo "not ok" 1 - "native CMYK8 multi-layer bad ICC failure trace is missing"
        exit 0
        ;;
esac

echo "ok" 1 - "native CMYK8 multi-layer bad ICC keeps embedded-ICC failure trace"
exit 0
