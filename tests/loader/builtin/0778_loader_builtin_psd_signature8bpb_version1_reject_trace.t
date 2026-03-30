#!/bin/sh
# Verify builtin PSD loader rejects 8BPB header with version=1 mismatch.
# Fixture generation commands:
#   cp tests/data/inputs/formats/stbi_minimal_version2_rgb_rle.psd \
#      tests/data/inputs/formats/stbi_minimal_8bpb_version1_rgb_rle.psd
#   printf '8BPB' | dd of=tests/data/inputs/formats/stbi_minimal_8bpb_version1_rgb_rle.psd \
#      bs=1 count=4 conv=notrunc
#   printf '\x00\x01' | dd of=tests/data/inputs/formats/stbi_minimal_8bpb_version1_rgb_rle.psd \
#      bs=1 seek=4 count=2 conv=notrunc

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/stbi_minimal_8bpb_version1_rgb_rle.psd"
trace_log=''

trace_log=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! \
    "${input_psd}" -o /dev/null 2>&1) && {
    echo "not ok" 1 - "8BPB+version1 was unexpectedly accepted"
    exit 0
}

case "${trace_log}" in
    *"builtin PSD: malformed section length/offset"*)
        echo "ok" 1 - "8BPB+version1 mismatch is rejected as malformed header"
        ;;
    *)
        echo "not ok" 1 - "expected mismatch rejection trace is missing"
        exit 0
        ;;
esac

exit 0
