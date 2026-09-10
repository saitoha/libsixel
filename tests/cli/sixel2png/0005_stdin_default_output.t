#!/bin/sh
# TAP test verifying stdin input produces valid PNG on stdout.

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

stdout_path="${ARTIFACT_LOCAL_DIR}/stdout.png"

${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -i - <"${TOP_SRCDIR}/images/map8.six" >"${stdout_path}" || {
    echo "not ok" 1 - "sixel2png without -o failed"
    exit 0
}

test -s "${stdout_path}" || {
    echo "not ok" 1 - "stdout png missing"
    exit 0
}

# The first 33 bytes cover the PNG signature and the complete IHDR chunk,
# including its CRC.  A signature-only check cannot detect a broken writer.
expected_header_cksum="517916970 33"
actual_header_cksum=$(dd bs=1 count=33 if="${stdout_path}" 2>/dev/null | cksum)

test "${actual_header_cksum}" = "${expected_header_cksum}" || {
    echo "not ok" 1 - "stdout png IHDR or CRC is invalid"
    exit 0
}

echo "ok" 1 - "default stdout PNG produced"
exit 0
