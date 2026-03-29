#!/bin/sh
# TAP test verifying orientation precedence:
# suboption > per-loader env > global env > default.

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

test "${ref_on}" != "${ref_off}" || {
    echo "not ok" 1 - "orientation on/off references were identical"
    exit 0
}

out_default=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Llibpng! "${input_png}" 2>/dev/null) || {
    echo "not ok" 1 - "default orientation decode failed"
    exit 0
}

test "${out_default}" = "${ref_on}" || {
    echo "not ok" 1 - "default orientation did not match ON reference"
    exit 0
}

out_global_off=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_LOADER_ORIENTATION=off" \
    -Llibpng! "${input_png}" 2>/dev/null) || {
    echo "not ok" 1 - "global orientation env decode failed"
    exit 0
}

test "${out_global_off}" = "${ref_off}" || {
    echo "not ok" 1 - "global orientation env did not match OFF reference"
    exit 0
}

out_per_loader=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_LOADER_ORIENTATION=off" \
    --env "SIXEL_LOADER_LIBPNG_ORIENTATION=on" \
    -Llibpng! "${input_png}" 2>/dev/null) || {
    echo "not ok" 1 - "per-loader orientation env decode failed"
    exit 0
}

test "${out_per_loader}" = "${ref_on}" || {
    echo "not ok" 1 - "per-loader orientation env did not override global env"
    exit 0
}

out_suboption=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_LOADER_ORIENTATION=on" \
    --env "SIXEL_LOADER_LIBPNG_ORIENTATION=on" \
    -Llibpng:orientation=off! "${input_png}" 2>/dev/null) || {
    echo "not ok" 1 - "suboption orientation decode failed"
    exit 0
}

test "${out_suboption}" = "${ref_off}" || {
    echo "not ok" 1 - "suboption orientation did not override env values"
    exit 0
}

echo "ok" 1 - "orientation precedence follows suboption > per-loader > global > default"
exit 0
