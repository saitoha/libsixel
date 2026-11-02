#!/bin/sh
# Policy: docs/testing/staticcheck.md
# Policy: docs/misc/platforms/emscripten.md
# Coverage: EM-04
# Guard the Node raw-filesystem stdio setup in both build descriptions.
set -eux
echo '1..1'
set -v
src_root=$1
awk '
/^[[:space:]]*(#|\/\/)/ { next }
FILENAME ~ /configure.ac$/ {
    if ($0 ~ /SIXEL_EMSCRIPTEN_STDIO=.*build-aux\/emscripten-node-stdio.js/) path = 1
    if ($0 ~ /EMSCRIPTEN_LDFLAGS=.*-sNODERAWFS=1 --pre-js \$SIXEL_EMSCRIPTEN_STDIO/)
        autotools = 1
}
FILENAME ~ /meson.build$/ {
    if ($0 ~ /emscripten_ldflags.*-sNODERAWFS=1.*--pre-js/) meson = 1
    if ($0 ~ /meson.current_source_dir.*build-aux\/emscripten-node-stdio.js/)
        meson_path = 1
}
FILENAME ~ /emscripten-node-stdio.js$/ {
    if ($0 ~ /process.versions.node/) node = 1
    if ($0 ~ /require\(.worker_threads.\).isMainThread/) main = 1
    if ($0 ~ /process.stdin, process.stdout, process.stderr/) streams = 1
    if ($0 ~ /stream._handle.setBlocking\(true\)/) blocking = 1
}
FILENAME ~ /Makefile.am$/ &&
    /^EXTRA_(img2sixel|sixel2png|lsqa|jpegref|progressive_study)_DEPENDENCIES = \$\(SIXEL_EMSCRIPTEN_STDIO\)$/ {
    dependencies[$1] = 1
}
FILENAME ~ /test-runner-link.am$/ && /\$\(SIXEL_EMSCRIPTEN_STDIO\)/ {
    runner = 1
}
FILENAME ~ /meson.build$/ && /link_depends: emscripten_link_depends/ {
    meson_dependencies[FILENAME] = 1
}
END {
    for (name in dependencies) count++
    for (name in meson_dependencies) meson_count++
    exit !(path && autotools && meson && meson_path &&
           node && main && streams && blocking && runner &&
           count == 5 && meson_count == 4)
}
' "$src_root/configure.ac" "$src_root/meson.build" \
    "$src_root/build-aux/emscripten-node-stdio.js" \
    "$src_root/build-aux/test-runner-link.am" \
    "$src_root/converters/Makefile.am" "$src_root/converters/meson.build" \
    "$src_root/assessment/Makefile.am" "$src_root/assessment/meson.build" \
    "$src_root/tools/Makefile.am" "$src_root/tools/meson.build" \
    "$src_root/tests/meson.build" || {
    echo 'not ok 1 - Node raw-filesystem stdio setup is incomplete'
    exit 0
}
echo 'ok 1 - Node raw-filesystem stdio setup is retained'
exit 0
