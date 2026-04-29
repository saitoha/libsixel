#!/bin/sh
# TAP test for WebP chain: gd failure reaches libwebp.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_WEBP-}" = 1 || {
    printf "1..0 # SKIP libwebp loader is unavailable\n"
    exit 0
}

test "${HAVE_DECL_GDIMAGECREATEFROMWEBPPTR-}" = 1 || {
    printf "1..0 # SKIP HAVE_DECL_GDIMAGECREATEFROMWEBPPTR is unavailable\n"
    exit 0
}

echo "1..1"
set -v

input_webp="${TOP_SRCDIR}/tests/data/inputs/snake_64.webp"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! -ldisable \
    "${input_webp}" >/dev/null || {
    printf "ok 1 # SKIP libwebp loader cannot decode WebP in this runtime\n"
    exit 0
}

trace_log=$(set +xv; head -c 64 "${input_webp}" | \
    SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} \
    "${IMG2SIXEL_PATH}" -L gd,libwebp! -ldisable - \
    2>&1 >/dev/null) && {
    echo "not ok 1 - gd,libwebp unexpectedly accepted truncated WebP"
    exit 0
}

after_gd=${trace_log#*LSXLOAD1|event=fail|loader=gd|code=L_ERR_GD*}
test "${after_gd}" != "${trace_log}" || {
    echo "not ok 1 - gd failure code was not reported"
    exit 0
}

after_libwebp=${after_gd#*LSXLOAD1|event=try|loader=libwebp|code=L_TRY*}
test "${after_libwebp}" != "${after_gd}" || {
    echo "not ok 1 - libwebp loader was not attempted after gd failure"
    exit 0
}

test "${after_libwebp#*LSXLOAD1|event=fail|loader=libwebp|code=*}" \
    != "${after_libwebp}" || {
    echo "not ok 1 - libwebp failure code was not reported"
    exit 0
}

echo "ok 1 - truncated WebP reaches libwebp after gd failure"
exit 0
