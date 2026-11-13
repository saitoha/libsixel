#!/bin/sh
# Native linker arguments must survive the GNU compiler-option adapter.
# Policy: docs/build/test-harness.md
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
fixture=$(mktemp -d "${TMPDIR:-/tmp}/sixel-compiler-link-XXXXXX")
trap 'rm -rf "$fixture"' EXIT HUP INT TERM
cat > "$fixture/clang-cl" <<'MOCK'
#!/bin/sh
set -eu
test "$#" -eq 7
test "$1" = '-Foshared dir/test.obj'
test "$2" = 'before.lib'
test "$3" = "$BOUNDARY"
test "$4" = -dll
test "$5" = '-LIBPATH:shared dir'
test "$6" = '-literal.lib'
test "$7" = native.lib
exit "${DRIVER_STATUS:-0}"
MOCK
chmod +x "$fixture/clang-cl"
for BOUNDARY in -link /link; do
    export BOUNDARY
    sh "$1/build-aux/compile" "$fixture/clang-cl" \
        -o 'shared dir/test.obj' -lbefore "$BOUNDARY" \
        -dll '-LIBPATH:shared dir' -literal.lib native.lib || {
        echo 'not ok 1 - native linker arguments retain order and boundaries'
        exit 0
    }
done
result=0
DRIVER_STATUS=37 sh "$1/build-aux/compile" "$fixture/clang-cl" \
    -o 'shared dir/test.obj' -lbefore "$BOUNDARY" \
    -dll '-LIBPATH:shared dir' -literal.lib native.lib || result=$?
test "$result" -eq 37 || {
    echo 'not ok 1 - native linker failure propagates through the adapter'
    exit 0
}
echo 'ok 1 - compiler adapter preserves the native linker boundary'
exit 0
