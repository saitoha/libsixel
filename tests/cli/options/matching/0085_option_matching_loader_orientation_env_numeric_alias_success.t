#!/bin/sh
# TAP test verifying numeric orientation env aliases 1/0 are accepted.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_LIBPNG-}" = 1 || {
    printf "1..0 # SKIP libpng is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/orientation_exif_o6_12x8.png"

ref_on=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Llibpng:orientation=on! "${input_png}" 2>/dev/null) || {
    echo "not ok" 1 - "orientation=on reference decode failed"
    exit 0
}

ref_off=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Llibpng:orientation=off! "${input_png}" 2>/dev/null) || {
    echo "not ok" 1 - "orientation=off reference decode failed"
    exit 0
}

out_global_1=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_LOADER_ORIENTATION=1" \
    -Llibpng! "${input_png}" 2>/dev/null) || {
    echo "not ok" 1 - "global orientation=1 decode failed"
    exit 0
}

test "${out_global_1}" = "${ref_on}" || {
    echo "not ok" 1 - "global orientation=1 did not match ON reference"
    exit 0
}

out_global_0=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_LOADER_ORIENTATION=0" \
    -Llibpng! "${input_png}" 2>/dev/null) || {
    echo "not ok" 1 - "global orientation=0 decode failed"
    exit 0
}

test "${out_global_0}" = "${ref_off}" || {
    echo "not ok" 1 - "global orientation=0 did not match OFF reference"
    exit 0
}

out_per_loader_1=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_LOADER_ORIENTATION=0" \
    --env "SIXEL_LOADER_LIBPNG_ORIENTATION=1" \
    -Llibpng! "${input_png}" 2>/dev/null) || {
    echo "not ok" 1 - "per-loader orientation=1 decode failed"
    exit 0
}

test "${out_per_loader_1}" = "${ref_on}" || {
    echo "not ok" 1 - "per-loader orientation=1 did not override global OFF"
    exit 0
}

out_per_loader_0=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_LOADER_ORIENTATION=1" \
    --env "SIXEL_LOADER_LIBPNG_ORIENTATION=0" \
    -Llibpng! "${input_png}" 2>/dev/null) || {
    echo "not ok" 1 - "per-loader orientation=0 decode failed"
    exit 0
}

test "${out_per_loader_0}" = "${ref_off}" || {
    echo "not ok" 1 - "per-loader orientation=0 did not override global ON"
    exit 0
}

echo "ok" 1 - "orientation env numeric aliases are accepted and respected"
exit 0
