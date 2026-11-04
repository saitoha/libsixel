#!/bin/sh
# Unity needs XSI selected before any included source reads system headers.
# Policy: docs/build/test-harness.md
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
awk '
function valid(text,    count, copy) {
    gsub(/[[:space:]]+/, " ", text)
    copy = text
    count = gsub(/c_args: test_feature_c_args \+/, "", copy)
    return count == 3 &&
        index(text, "test_feature_c_args = libsixel_posix_defs if host_system != \047dragonfly\047 test_feature_c_args += cc.get_supported_arguments(\047-D_XOPEN_SOURCE=700\047) endif")
}
!/^[[:space:]]*#/ { text = text $0 "\n" }
END {
    missing = text
    sub(/c_args: test_feature_c_args/, "c_args: libsixel_posix_defs", missing)
    inverted = text
    sub(/host_system !=/, "host_system ==", inverted)
    late = text
    sub(/-D_XOPEN_SOURCE=700/, "", late)
    exit !valid(text) || valid(missing) || valid(inverted) || valid(late)
}
' "$1/tests/meson.build" || {
    echo 'not ok' '1 - unity test targets select their namespace before headers'
    exit 0
}
echo 'ok' '1 - unity test targets select their namespace before headers'
exit 0
