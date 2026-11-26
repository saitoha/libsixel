#!/bin/sh
# Reject forced GPU application in linear RGB before truncation.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf '1..0 # SKIP img2sixel is disabled\n'
    exit 0
}

printf '1..1\n'
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
fixture="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
target="${ARTIFACT_LOCAL_DIR}/output.six"
cat "$fixture" >"$target"
status=0
trace=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -7 '-Lbuiltin!' \
    -o "$target" -Gforce -Wlinear "$fixture" \
    2>&1 >"${ARTIFACT_LOCAL_DIR}/stdout") || status=$?
test "$status" -eq 2 && test ! -s "${ARTIFACT_LOCAL_DIR}/stdout" || {
    printf '\n%s\n' 'not ok 1 - expected rejection before output'
    exit 0
}
case "$trace" in
    *"forced GPU palette application requires -W gamma."*) true ;;
    *) false ;;
esac || {
    printf '\n%s\n' 'not ok 1 - expected rejection diagnostic missing'
    exit 0
}
cmp -s "$fixture" "$target" || {
    printf '\n%s\n' 'not ok 1 - existing output contents changed'
    exit 0
}
printf '\n%s\n' 'ok 1 - rejection preserves the existing output'
exit 0
