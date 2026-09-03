#!/bin/sh
# Preserve the img2sixel diagnostic below the color-count minimum.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --colors=0 \
    2>&1) && {
    echo "not ok 1 - img2sixel accepts zero colors"
    exit 0
}
test "${msg#*-p/--colors*parameter*must*be*1*or*more.}" != "${msg}" || {
    echo "not ok 1 - colors minimum diagnostic changed"
    printf '%s\n' "${msg}" >&2
    exit 0
}

echo "ok 1 - colors minimum diagnostic is preserved"
exit 0
