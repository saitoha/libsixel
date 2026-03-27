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

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/tests/data/corrupted/truncated.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/gdk_corrupted_png.sixel"
error_log="${ARTIFACT_LOCAL_DIR}/gdk_corrupted_png.err"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L gdk-pixbuf2! "${input_png}" >"${output_sixel}" 2>"${error_log}" && {
    echo "not ok" 1 - "forced gdk-pixbuf2 corrupted PNG should fail"
    exit 0
}

grep -E "load_with_gdkpixbuf: generic loader (write|close) failed" \
    "${error_log}" >/dev/null || {
    echo "not ok" 1 - "corrupted PNG failure did not include gdk-pixbuf diagnostics"
    exit 0
}

echo "ok" 1 - "forced gdk-pixbuf2 corrupted PNG is rejected"

exit 0
