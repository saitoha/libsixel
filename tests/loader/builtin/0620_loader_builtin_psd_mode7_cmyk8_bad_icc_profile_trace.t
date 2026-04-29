#!/bin/sh
# Verify builtin PSD mode7(4ch->CMYK) logs ICC conversion failure for invalid profile bytes.
# Fixture generation command:
#   python3 tests/data/inputs/formats/generate_psd_policy_trace_fixtures.py

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/stbi_minimal_mode7_cmyk8_bad_icc_profile.psd"

trace_log=$(set +xv; SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin:cms_engine=auto! \
    "${input_psd}" -o /dev/null 2>&1 || true)

case "${trace_log}" in
    *"libsixel: builtin PSD: embedded ICC conversion failed"*)
        ;;
    *)
        echo "not ok" 1 - "missing mode7(4ch->CMYK) embedded-ICC conversion failure trace"
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

echo "ok" 1 - "builtin PSD mode7(4ch->CMYK) logs ICC conversion failure for invalid profile bytes"
exit 0
