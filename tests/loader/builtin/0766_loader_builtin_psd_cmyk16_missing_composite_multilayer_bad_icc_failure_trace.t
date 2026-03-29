#!/bin/sh
# Verify native CMYK16 multi-layer fallback with bad ICC bytes does not
# misclassify resource parsing as malformed and still decodes.
# Fixture generation command:
#   python3 tests/data/inputs/formats/generate_psd_policy_trace_fixtures.py

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/snake16_cmyk16_missing_composite_multilayer_normal_bad_icc_profile.psd"
trace_log=''
command_status=0

trace_log=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -v -Lbuiltin:cms=auto! \
    "${input_psd}" -o /dev/null 2>&1) || command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "native CMYK16 multi-layer bad ICC decode failed: ${trace_log}"
    exit 0
}

case "${trace_log}" in
    *"malformed ICC resource section; skipping ICC conversion"*)
        echo "not ok" 1 - "native CMYK16 multi-layer bad ICC was misclassified as malformed resource"
        exit 0
        ;;
esac

echo "ok" 1 - "native CMYK16 multi-layer bad ICC decodes without malformed-resource misclassification"
exit 0
