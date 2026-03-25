#!/bin/sh
# Verify malformed PSD image-resource signature does not break CMS decode path.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/stbi_minimal_bad_resource_signature.psd"

run_img2sixel -L builtin:cms_engine=auto! "${input_psd}" >/dev/null || {
    echo "not ok" 1 - "builtin loader failed on PSD with malformed resource signature"
    exit 0
}

echo "ok" 1 - "builtin loader decodes PSD even when resource signature is malformed"
exit 0
