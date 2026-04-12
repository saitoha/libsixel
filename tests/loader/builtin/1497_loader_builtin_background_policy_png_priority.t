#!/bin/sh
# Verify PNG bKGD priority policy, strict parsing, and fallback behavior.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_png="${TOP_SRCDIR}/images/pngsuite/background/bgbn4a08.png"

out_default=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Lbuiltin:cms_engine=none! -B#fff -d fs:scan=raster "${input_png}") || {
    echo "not ok 1 - default background policy render failed"
    exit 0
}

out_file_first=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_BACKGROUND_POLICY=file_first \
    -Lbuiltin:cms_engine=none! -B#fff -d fs:scan=raster "${input_png}") || {
    echo "not ok 1 - file_first background policy render failed"
    exit 0
}

out_invalid=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_BACKGROUND_POLICY=invalid \
    -Lbuiltin:cms_engine=none! -B#fff -d fs:scan=raster "${input_png}") || {
    echo "not ok 1 - invalid background policy render failed"
    exit 0
}

out_explicit_first=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_BACKGROUND_POLICY=explicit_first \
    -Lbuiltin:cms_engine=none! -B#fff -d fs:scan=raster "${input_png}") || {
    echo "not ok 1 - explicit_first background policy render failed"
    exit 0
}

test "${out_default}" = "${out_file_first}" || {
    echo "not ok 1 - default policy mismatch against file_first"
    exit 0
}

test "${out_default}" = "${out_invalid}" || {
    echo "not ok 1 - invalid background policy did not fall back to file_first"
    exit 0
}

test "${out_default}" != "${out_explicit_first}" || {
    echo "not ok 1 - explicit_first did not override file background priority"
    exit 0
}

echo "ok 1 - PNG background policy priority and fallback verified"
exit 0
