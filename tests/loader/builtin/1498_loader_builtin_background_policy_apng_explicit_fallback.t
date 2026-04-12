#!/bin/sh
# Verify APNG background policy fallback when only explicit background exists.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_dispose_background.png"

out_default=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Lbuiltin! -B#fff -d fs:scan=raster "${input_png}") || {
    echo "not ok 1 - default APNG background render failed"
    exit 0
}

out_file_first=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_BACKGROUND_POLICY=file_first \
    -Lbuiltin! -B#fff -d fs:scan=raster "${input_png}") || {
    echo "not ok 1 - file_first APNG background render failed"
    exit 0
}

out_explicit_first=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_BACKGROUND_POLICY=explicit_first \
    -Lbuiltin! -B#fff -d fs:scan=raster "${input_png}") || {
    echo "not ok 1 - explicit_first APNG background render failed"
    exit 0
}

out_invalid=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_BACKGROUND_POLICY=oops \
    -Lbuiltin! -B#fff -d fs:scan=raster "${input_png}") || {
    echo "not ok 1 - invalid APNG background policy render failed"
    exit 0
}

out_no_bg=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Lbuiltin! -d fs:scan=raster "${input_png}") || {
    echo "not ok 1 - APNG no-bg render failed"
    exit 0
}

test "${out_default}" = "${out_file_first}" || {
    echo "not ok 1 - default policy mismatch against file_first for APNG"
    exit 0
}

test "${out_default}" = "${out_explicit_first}" || {
    echo "not ok 1 - explicit_first changed APNG without file background"
    exit 0
}

test "${out_default}" = "${out_invalid}" || {
    echo "not ok 1 - invalid background policy did not fall back for APNG"
    exit 0
}

test "${out_default}" != "${out_no_bg}" || {
    echo "not ok 1 - explicit APNG background was not applied"
    exit 0
}

echo "ok 1 - APNG background fallback and policy parsing verified"
exit 0
