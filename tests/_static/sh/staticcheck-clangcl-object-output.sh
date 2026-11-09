#!/bin/sh
# Keep clang-cl coverage names tied to unique object outputs.
# Policy: docs/build/autotools.md
# Policy: docs/build/test-harness.md
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
src_root=$1
# The mock observes the real compiler adapter without compiling any C.
fixture=$(mktemp -d "${TMPDIR:-/tmp}/sixel-clangcl-output-XXXXXX")
trap 'rm -rf "$fixture"' 0
trap 'exit 1' 1 2 3 15
awk '
/^for sixel_cc_driver in \$CC; do$/ { active = 1 }
active { print }
active && /^esac$/ { active = 0; found++ }
END { exit found != 1 }
' "$src_root/configure.ac" > "$fixture/route.sh" || {
    echo 'not ok 1 - clang-cl output adapter registration'
    exit 0
}
cat > "$fixture/clang-cl" <<'MOCK'
#!/bin/sh
output=
for arg; do
    case $arg in
        -o) exit 42 ;;
        -Fo*) output=${arg#-Fo} ;;
    esac
done
case $output in
    'one out/common.obj'|'one out\common.obj') echo one ;;
    'two out/common.obj'|'two out\common.obj') echo two ;;
    *) exit 43 ;;
esac
MOCK
chmod +x "$fixture/clang-cl"
CC="$fixture/clang-cl"
SHELL="sh"
# Used by the extracted configure fragment.
# shellcheck disable=SC2034
am_aux_dir="$src_root/build-aux"
# shellcheck disable=SC1091
. "$fixture/route.sh"
# CC is the configured command and intentionally includes adapter arguments.
# shellcheck disable=SC2086
first=$($CC -c a/common.c -o 'one out/common.obj') || {
    echo 'not ok 1 - clang-cl first object output'
    exit 0
}
# shellcheck disable=SC2086
second=$($CC -c b/common.c -o 'two out/common.obj') || {
    echo 'not ok 1 - clang-cl second object output'
    exit 0
}
test "$first:$second" = one:two || {
    echo 'not ok 1 - clang-cl object outputs must remain distinct'
    exit 0
}
wrapped=$CC
# shellcheck disable=SC1091
. "$fixture/route.sh"
test "$CC" = "$wrapped" || {
    echo 'not ok 1 - clang-cl adapter must not be duplicated'
    exit 0
}
echo 'ok 1 - clang-cl coverage follows distinct native object outputs'
exit 0
