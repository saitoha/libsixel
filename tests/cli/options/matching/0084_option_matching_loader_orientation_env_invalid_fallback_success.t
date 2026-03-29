#!/bin/sh
# TAP test verifying invalid orientation env values fall back safely.

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

out_invalid_global=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_LOADER_ORIENTATION=invalid-token" \
    -Llibpng! "${input_png}" 2>/dev/null) || {
    echo "not ok" 1 - "invalid global orientation env decode failed"
    exit 0
}

test "${out_invalid_global}" = "${ref_on}" || {
    echo "not ok" 1 - "invalid global orientation env did not fallback to default ON"
    exit 0
}

out_invalid_per_loader=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_LOADER_ORIENTATION=off" \
    --env "SIXEL_LOADER_LIBPNG_ORIENTATION=invalid-token" \
    -Llibpng! "${input_png}" 2>/dev/null) || {
    echo "not ok" 1 - "invalid per-loader orientation env decode failed"
    exit 0
}

test "${out_invalid_per_loader}" = "${ref_off}" || {
    echo "not ok" 1 - "invalid per-loader env did not fallback to global OFF"
    exit 0
}

echo "ok" 1 - "invalid orientation env values fallback to lower-priority source"
exit 0
