#!/bin/sh
# Validate heckbert profile suboption parsing.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..4"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Q "heckbert:profile=compat" \
    -p 16 \
    "${input_image}" >/dev/null || {
    echo "not ok" 1 - "heckbert profile=compat rejected"
    exit 0
}
echo "ok" 1 - "heckbert profile=compat accepted"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Q "heckbert:profile=speed" \
    -p 16 \
    "${input_image}" >/dev/null || {
    echo "not ok" 2 - "heckbert profile=speed rejected"
    exit 0
}
echo "ok" 2 - "heckbert profile=speed accepted"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Q "heckbert:profile=quality" \
    -p 16 \
    "${input_image}" >/dev/null || {
    echo "not ok" 3 - "heckbert profile=quality rejected"
    exit 0
}
echo "ok" 3 - "heckbert profile=quality accepted"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Q "heckbert:profile=invalid" \
    -p 16 \
    "${input_image}" >/dev/null 2>/dev/null && {
    echo "not ok" 4 - "heckbert invalid profile accepted"
    exit 0
}
echo "ok" 4 - "heckbert invalid profile rejected"

exit 0
