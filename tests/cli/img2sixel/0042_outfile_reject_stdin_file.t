#!/bin/sh
# Reject a named SIXEL output that is also redirected stdin.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf '1..0 # SKIP img2sixel is disabled\n'
    exit 0
}

printf '1..1\n'
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
fixture="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
input="${ARTIFACT_LOCAL_DIR}/$$-input.six"
cat "$fixture" >"$input"
status=0
# The command must reject this alias before it writes to the file.
# shellcheck disable=SC2094
trace=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -7 '-Lbuiltin!' \
    -o "$input" 2>&1 >"${ARTIFACT_LOCAL_DIR}/stdout" <"$input") || status=$?
cmp -s "$fixture" "$input" || {
    printf '\n%s\n' 'not ok 1 - redirected standard input contents changed'
    exit 0
}
test "$status" -eq 2 && test ! -s "${ARTIFACT_LOCAL_DIR}/stdout" || {
    printf '\n%s\n' 'not ok 1 - redirected standard input was not rejected before output'
    exit 0
}
case "$trace" in
    *"Input and output refer to the same file."*) true ;;
    *) false ;;
esac || {
    printf '\n%s\n' 'not ok 1 - redirected standard input diagnostic missing'
    exit 0
}
printf '\n%s\n' 'ok 1 - redirected standard input is rejected and preserved'
exit 0
