#!/bin/sh
# Preserve the img2sixel diagnostic for an invalid thread token.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=bogus \
    2>&1) && {
    echo "not ok 1 - img2sixel accepts an invalid thread token"
    exit 0
}
test "${msg#*threads*accepts*positive*integers*or*[\']auto[\'].}" != \
    "${msg}" || {
    echo "not ok 1 - img2sixel thread diagnostic changed"
    printf '%s\n' "${msg}" >&2
    exit 0
}

echo "ok 1 - img2sixel thread diagnostic is preserved"
exit 0
