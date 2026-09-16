#!/bin/sh
# TAP test verifying SIXEL_LOADER_LIBTIFF_CMS_ENGINE overrides global setting.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

test "${HAVE_LIBTIFF-}" = 1 || {
    printf "1..0 # SKIP libtiff loader is unavailable\n";
    exit 0
}


echo "1..1"
set -v

input_tiff="${TOP_SRCDIR}/tests/data/inputs/formats/snake-64-embedded-esrgb.tiff"
ref_cms1_cksum=''
ref_cms0_cksum=''
override_cksum=''

set +x
ref_cms1_cksum=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Llibtiff:cms_engine=auto! "${input_tiff}" | cksum) || {
    set -x
    echo "not ok" 1 - "libtiff cms=1 reference decode failed"
    exit 0
}

ref_cms0_cksum=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Llibtiff:cms_engine=none! "${input_tiff}" | cksum) || {
    set -x
    echo "not ok" 1 - "libtiff cms=0 reference decode failed"
    exit 0
}

override_cksum=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_LOADER_CMS_ENGINE=none" \
    --env "SIXEL_LOADER_LIBTIFF_CMS_ENGINE=auto" \
    -Llibtiff! "${input_tiff}" | cksum) || {
    set -x
    echo "not ok" 1 - "SIXEL_LOADER_LIBTIFF_CMS_ENGINE override decode failed"
    exit 0
}
set -x

test "${override_cksum}" = "${ref_cms1_cksum}" || {
    echo "not ok" 1 - "SIXEL_LOADER_LIBTIFF_CMS_ENGINE did not match cms=1 reference"
    exit 0
}

test "${override_cksum}" != "${ref_cms0_cksum}" || {
    echo "not ok" 1 - "override output did not differ from cms=0 baseline"
    exit 0
}

echo "ok" 1 - "SIXEL_LOADER_LIBTIFF_CMS_ENGINE overrides global none"
exit 0
