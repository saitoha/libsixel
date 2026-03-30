#!/bin/sh
# Verify builtin PSD loader accepts 8BPB+version2 minimal RLE composite input.
# Fixture generation commands:
#   cp tests/data/inputs/formats/stbi_minimal_version2_rgb_rle.psd \
#      tests/data/inputs/formats/stbi_minimal_8bpb_version2_rgb_rle.psd
#   printf '8BPB' | dd of=tests/data/inputs/formats/stbi_minimal_8bpb_version2_rgb_rle.psd \
#      bs=1 count=4 conv=notrunc

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/stbi_minimal_8bpb_version2_rgb_rle.psd"
trace_log=''
command_status=0

trace_log=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! \
    "${input_psd}" -o /dev/null 2>&1) || command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "8BPB+version2 minimal RLE failed to decode: ${trace_log}"
    exit 0
}

case "${trace_log}" in
    *"builtin PSD: unsupported version"*|*"builtin PSD: malformed section length/offset"*)
        echo "not ok" 1 - "8BPB+version2 was not treated as valid PSB header"
        exit 0
        ;;
    *)
        echo "ok" 1 - "8BPB+version2 minimal RLE decodes successfully"
        ;;
esac

exit 0
