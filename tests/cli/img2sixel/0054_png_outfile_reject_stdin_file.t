#!/bin/sh
# Preserve redirected stdin when it is also the named PNG output.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf '1..0 # SKIP img2sixel is disabled\n'
    exit 0
}

printf '1..1\n'
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
fixture="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
input="${ARTIFACT_LOCAL_DIR}/$$-input.png"
cat "$fixture" >"$input"
status=0
trace=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -7 '-Lbuiltin!' \
    -o "png:$input" 2>&1 >/dev/null <"$input") || status=$?
cmp -s "$fixture" "$input" || {
    printf '\n%s\n' 'not ok 1 - PNG output changed redirected standard input'
    exit 0
}
test "$status" -eq 2 || {
    printf '\n%s\n' 'not ok 1 - PNG output did not reject redirected stdin'
    exit 0
}
case "$trace" in
    *"Input and output refer to the same file."*) true ;;
    *"Cannot overwrite an existing output file when standard input cannot be identified."*) true ;;
    *) false ;;
esac || {
    printf '\n%s\n' 'not ok 1 - redirected standard input diagnostic missing'
    exit 0
}
printf '\n%s\n' 'ok 1 - PNG output rejects and preserves redirected stdin'
exit 0
