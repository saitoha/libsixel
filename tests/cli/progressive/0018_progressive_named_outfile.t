#!/bin/sh
# Compare file presentation with a redirected standard output capture.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf '1..0 # SKIP img2sixel is disabled\n'
    exit 0
}

printf '1..1\n'
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
input="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
expected="${ARTIFACT_LOCAL_DIR}/expected.six"
target="${ARTIFACT_LOCAL_DIR}/output.six"
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -7 '-Lbuiltin!' -Goff \
    -Zprogressive:N2:M0 "$input" >"$expected" || {
    printf '\n%s\n' 'not ok 1 - reference presentation failed'
    exit 0
}
test -s "$expected" || {
    printf '\n%s\n' 'not ok 1 - reference presentation was empty'
    exit 0
}
cat "$expected" "$expected" >"$target"
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -7 '-Lbuiltin!' -Goff \
    -Zprogressive:N2:M0 -o "$target" "$input" || {
    printf '\n%s\n' 'not ok 1 - named file presentation failed'
    exit 0
}
cmp -s "$expected" "$target" || {
    printf '\n%s\n' 'not ok 1 - named file presentation differs from reference'
    exit 0
}
printf '\n%s\n' 'ok 1 - progressive presentation replaces the named file'
exit 0
