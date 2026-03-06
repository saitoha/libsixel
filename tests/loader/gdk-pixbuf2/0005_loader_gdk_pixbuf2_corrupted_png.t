#!/bin/sh
# TAP test confirming forced gdk-pixbuf2 decoding rejects corrupted PNG input.

# Enable strict mode with verbose tracing for diagnostics.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

test "${HAVE_GDK_PIXBUF2-}" = 1 || {
    printf "1..0 # SKIP gdk-pixbuf2 support is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
echo "1..1"
set -v
mkdir "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/tests/data/corrupted/truncated.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/gdk_corrupted_png.sixel"

run_img2sixel -L gdk-pixbuf2! "${input_png}" >"${output_sixel}" && {
    echo "not ok" 1 - "forced gdk-pixbuf2 corrupted PNG should fail"
    exit 0
}

echo "ok" 1 - "forced gdk-pixbuf2 corrupted PNG is rejected"

exit 0
