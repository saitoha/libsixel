#!/bin/sh
# Policy: docs/testing/staticcheck.md
# Meson graph: docs/build/meson.md
# Policy: docs/build/autotools.md
# CPU discovery needs DragonFly's BSD namespace in both build paths.
set -eux
echo '1..1'
set -v
awk '
function valid(meson, generator,    copies) {
    gsub(/[[:space:]]+/, " ", meson)
    copies = meson
    return gsub(/-D_POSIX_C_SOURCE=200809L/, "", copies) == 1 &&
        index(meson, "if host_system != \047dragonfly\047 libsixel_posix_defs += cc.get_supported_arguments([ \047-D_POSIX_C_SOURCE=200809L\047, ]) endif") &&
        index(generator, "#if !defined(_POSIX_C_SOURCE) && !defined(__DragonFly__)\n#define _POSIX_C_SOURCE 200809L\n#endif") &&
        index(generator, "#if !defined(_XOPEN_SOURCE) && !defined(__DragonFly__)\n#define _XOPEN_SOURCE 700\n#endif")
}
FILENAME == ARGV[1] && !/^[[:space:]]*#/ { meson = meson $0 "\n" }
FILENAME == ARGV[2] { generator = generator $0 "\n" }
END {
    unguarded = meson
    sub(/host_system != \047dragonfly\047/,
        "host_system == \047dragonfly\047", unguarded)
    posix = generator
    sub(/!defined\(_POSIX_C_SOURCE\) && !defined\(__DragonFly__\)/,
        "!defined(_POSIX_C_SOURCE)", posix)
    xopen = generator
    sub(/!defined\(_XOPEN_SOURCE\) && !defined\(__DragonFly__\)/,
        "!defined(_XOPEN_SOURCE)", xopen)
    exit !valid(meson, generator) || valid(unguarded, generator) ||
        valid(meson, posix) || valid(meson, xopen) ||
        valid(meson " -D_POSIX_C_SOURCE=200809L", generator)
}
' "$1/meson.build" "$1/tools/gen-amalgamation.sh" || {
    echo 'not ok' '1 - DragonFly CPU discovery keeps the BSD namespace'
    exit 0
}
echo 'ok' '1 - DragonFly CPU discovery keeps the BSD namespace'
exit 0
