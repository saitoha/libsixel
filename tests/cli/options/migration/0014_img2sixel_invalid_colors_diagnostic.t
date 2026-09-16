#!/bin/sh
# Preserve the img2sixel diagnostic for a non-numeric color count.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --colors=bogus \
    2>&1) && {
    echo "not ok 1 - img2sixel accepts a non-numeric color count"
    exit 0
}
test "${msg#*cannot*parse*-p/--colors*option.}" != "${msg}" || {
    echo "not ok 1 - invalid colors diagnostic changed"
    printf '%s\n' "${msg}" >&2
    exit 0
}

echo "ok 1 - invalid colors diagnostic is preserved"
exit 0
