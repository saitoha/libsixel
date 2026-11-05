#!/bin/sh
# A large MSYS2 archive crosses the native boundary through ordered files.
# Policy: docs/build/test-harness.md
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
src_root=$1
# These files are protocol fixtures inspected by the mock native archiver.
fixture=$(mktemp -d "${TMPDIR:-/tmp}/sixel-msvc-response-test-XXXXXX")
trap 'rm -rf "$fixture"' 0
trap 'exit 1' 1 2 3 15
cat > "$fixture/cygpath" <<'MOCK'
#!/bin/sh
set -eu
test "$1" = -w
printf '%s\n' "$2"
MOCK
cat > "$fixture/lib" <<'MOCK'
#!/bin/sh
set -eu
# Two direct groups surround an existing command file; none may be nested.
test "$#" = 3
test "$2" = '@existing command.rsp'
awk '
BEGIN { padding = sprintf("%0100d", 0) }
NR == 1 { if ($0 != "\"/NOLOGO\"") exit 1; next }
NR == 2 { if ($0 != "\"/OUT:C:\\space dir\\cases.lib\"") exit 1; next }
NR >= 3 && NR <= 4585 {
    if ($0 != "\"objects/" padding (NR - 3) ".o\"") exit 1
    next
}
{ exit 1 }
END { if (NR != 4585) exit 1 }
' "${1#@}"
# Fixed expectations exercise spaces, empty args, quotes and trailing slashes.
awk '
NR == 1 { if ($0 != "\"tail with spaces.o\"") exit 1; next }
NR == 2 { if ($0 != "\"\"") exit 1; next }
NR == 3 { if ($0 != "\"/LIBPATH:C:\\tail\\\\\"") exit 1; next }
NR == 4 { if ($0 != "\"/EXPORT:a\\\"b\"") exit 1; next }
{ exit 1 }
END { if (NR != 4) exit 1 }
' "${3#@}"
# The adapter must retain the real archiver status while cleaning its files.
exit 23
MOCK
chmod +x "$fixture/cygpath" "$fixture/lib"
# Generate a single invocation: repeatedly appending thousands of positional
# arguments would make fixture construction quadratic and inflate traces.
awk 'BEGIN {
    print "exec sh \"$1/build-aux/msvc-lib-response.sh\" \"$2/lib\" \\"
    print "\047/NOLOGO\047 \047/OUT:C:\\space dir\\cases.lib\047 \\"
    padding = sprintf("%0100d", 0)
    for (i = 0; i < 4583; ++i)
        print "\047objects/" padding i ".o\047 \\"
    print "\047@existing command.rsp\047 \047tail with spaces.o\047 \047\047 \\"
    print "\047/LIBPATH:C:\\tail\\\047 \047/EXPORT:a\"b\047"
}' > "$fixture/invoke"
status=0
PATH="$fixture:$PATH" TMPDIR="$fixture" \
    sh "$fixture/invoke" "$src_root" "$fixture" || status=$?
test "$status" = 23 || {
    echo 'not ok 1 - bounded native archive transport or status'
    exit 0
}
set -- "$fixture"/sixel-msvc-lib-*
test ! -e "$1" || {
    echo 'not ok 1 - native archive transport cleanup'
    exit 0
}
# Check the actual CI route as well as the adapter. A bypass would pass the
# protocol fixture but restore the unbounded native invocation in production.
awk '
/MSVC_LIB_RESPONSE="\$PWD\/build-aux\/msvc-lib-response.sh"/ { path++ }
/export MSVC_LIB_RESPONSE/ { exported++ }
/if test "\$tool" = lib; then/ { branch = 1; next }
branch {
    if ($0 ~ /exec sh "\$MSVC_LIB_RESPONSE" "\$real_tool" "\$\{converted\[@\]\}"/)
        routed++
    branch = 0
}
END { exit !(path == 1 && exported == 1 && routed == 1) }
' "$src_root/.github/actions/ci-steps/action.yml" || {
    echo 'not ok 1 - MSYS2 CI must use bounded archive transport'
    exit 0
}
echo 'ok 1 - bounded native archive transport'
exit 0
