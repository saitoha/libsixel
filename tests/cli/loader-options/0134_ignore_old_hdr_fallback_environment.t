#!/bin/sh
# An obsolete HDR profile alias must leave the default in effect.
set -eux

echo "1..1"
set -v

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf '\n%s\n' 'ok 1 # SKIP img2sixel disabled'
    exit 0
}
trace=$(set +xv; SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --cms-engine=builtin --env SIXEL_LOADER_HDR_FALLBACK_PROFILE=gamma-srgb \
    '-Lbuiltin:Tsrgb-linear!' \
    "${TOP_SRCDIR}/tests/data/inputs/formats/stbi_gradient16x16.hdr" \
    2>&1 >/dev/null) || {
    printf '\n%s\n' 'not ok 1 - HDR load failed'
    exit 0
}
case "${trace}" in
    *'tonemap=none fallback=srgb-linear'*) true ;;
    *) false ;;
esac || {
    printf '\n%s\n' 'not ok 1 - obsolete environment value changed the HDR profile'
    exit 0
}
printf '\n%s\n' 'ok 1 - default HDR profile survives obsolete environment value'
exit 0
