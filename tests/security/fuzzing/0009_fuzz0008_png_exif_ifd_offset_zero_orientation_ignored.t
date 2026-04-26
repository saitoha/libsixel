#!/bin/sh
# TAP test for fuzz0008: malformed PNG eXIf offset must be ignored safely.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +x

fuzz_seed="${TOP_SRCDIR}/tests/data/security/fuzzing/data/fuzz0008/png_exif_ifd_underflow_min.bin"
input_png="${TOP_SRCDIR}/tests/data/security/fuzzing/data/fuzz0008/png_exif_ifd_offset_zero_orientation6.png"
output_on="${ARTIFACT_ROOT}/${0##*/}.on.png"
output_off="${ARTIFACT_ROOT}/${0##*/}.off.png"
size=0
dims_on=''
dims_off=''

# The minimized fuzz seed remains in-tree so future fuzz sessions start from
# the reduced reproducer instead of re-discovering it from larger samples.
test -f "${fuzz_seed}" || {
    echo "not ok" 1 - "fuzz0008 minimized seed is missing"
    exit 0
}
size=$(wc -c < "${fuzz_seed}")
test "${size}" -eq 35 || {
    echo "not ok" 1 - "fuzz0008 minimized seed size drifted"
    exit 0
}

# This PNG keeps pixel data valid but carries a malformed eXIf TIFF header
# where IFD0 offset is zero. A strict parser must ignore this metadata.
test -f "${input_png}" || {
    echo "not ok" 1 - "fuzz0008 malformed PNG input is missing"
    exit 0
}
size=$(wc -c < "${input_png}")
test "${size}" -eq 733 || {
    echo "not ok" 1 - "fuzz0008 malformed PNG fixture size drifted"
    exit 0
}

SIXEL_LOADER_BUILTIN_ORIENTATION=on
export SIXEL_LOADER_BUILTIN_ORIENTATION
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -L builtin! -o "${output_on}" "${input_png}" >/dev/null || {
    echo "not ok" 1 - "fuzz0008 malformed PNG decode failed (orientation on)"
    exit 0
}

SIXEL_LOADER_BUILTIN_ORIENTATION=off
export SIXEL_LOADER_BUILTIN_ORIENTATION
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -L builtin! -o "${output_off}" "${input_png}" >/dev/null || {
    echo "not ok" 1 - "fuzz0008 malformed PNG decode failed (orientation off)"
    exit 0
}

dims_on=''
for token in $(od -tx1 -j16 -N8 "${output_on}"); do
    test "${token#??}" = "" || continue
    dims_on="${dims_on}${token}"
done
dims_off=''
for token in $(od -tx1 -j16 -N8 "${output_off}"); do
    test "${token#??}" = "" || continue
    dims_off="${dims_off}${token}"
done

test "${dims_on}" = "0000000c00000008" || {
    echo "not ok" 1 - "fuzz0008 malformed eXIf was incorrectly applied"
    exit 0
}
test "${dims_off}" = "0000000c00000008" || {
    echo "not ok" 1 - "fuzz0008 orientation-off baseline changed unexpectedly"
    exit 0
}

echo "ok" 1 - "fuzz0008 malformed PNG eXIf is ignored without regression"


exit 0
