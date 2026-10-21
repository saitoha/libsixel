#!/bin/sh
# Verify per-file state, diagnostics and inventory failures in batched checks.
# Policy: docs/testing/staticcheck.md
set -eux

echo '1..1'
set -v
src_root=$1
tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-c-source-batches-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM
mkdir -p "$tmpdir/root with spaces/src" "$tmpdir/bin"
control_root="$tmpdir/root with spaces"
printf '%s\n' 'gitdir: control' > "$control_root/.git"
CONTROL_INVENTORY="$tmpdir/inventory"
export CONTROL_INVENTORY
cat > "$tmpdir/bin/git" <<'GIT'
#!/bin/sh
exclude=0
for arg do
    case "$arg" in ':(exclude)*.inc.c') exclude=1 ;; esac
done
while IFS= read -r path; do
    case "$exclude:$path" in 1:*.inc.c) continue ;; esac
    printf '%s\n' "$path"
done < "$CONTROL_INVENTORY"
GIT
chmod +x "$tmpdir/bin/git"
# One generated corpus covers the 63/64 boundary, per-file duplicate
# suppression, line numbers, non-identifiers, empty/deleted files and providers
# before and after use. Literal comments retain their existing check semantics.
awk -v root="$control_root/src" -v expected="$tmpdir/identifier.expected" '
BEGIN {
    short = sprintf("%063d", 0)
    gsub(/0/, "a", short)
    long = short "a"
    print short > (root "/valid.c")
    print "9" long > (root "/valid.c")
    print "padding\n" long " " long > (root "/bad one.c")
    print long > (root "/bad one.c")
    print "padding\npadding\n" long > (root "/bad-two.c")
    printf "" > (root "/empty.c")
    print "#include <stdint.h>\nSIZE_MAX" > (root "/stdint.c")
    print "SIZE_MAX\n#include <limits.h>" > (root "/limits.c")
    print "#define SIZE_MAX (123)" > (root "/fallback.c")
    print "#define SIZE_MAX(x) (x)" > (root "/function.c")
    print "#define SIZE_MAX" > (root "/bare.c")
    print "SIZE_MAX_SUFFIX PRE_SIZE_MAX" > (root "/unused.c")
    print "SIZE_MAX" > (root "/missing.c")
    print "SIZE_MAX" > (root "/excluded.inc.c")
    print "1..1" > expected
    print "# src/bad one.c:2: identifier exceeds 63 significant chars: " long " (len=64)" > expected
    print "# src/bad-two.c:3: identifier exceeds 63 significant chars: " long " (len=64)" > expected
    print "not ok 1 - C identifiers stay within C99 significant-length limit" > expected
}'
printf '%s\n' src/valid.c 'src/bad one.c' src/empty.c src/bad-two.c \
    src/deleted.c > "$CONTROL_INVENTORY"
status=0
PATH="$tmpdir/bin:$PATH" sh \
    "$src_root/tests/_static/sh/staticcheck-c-identifier-significant-length.sh" \
    "$control_root" > "$tmpdir/actual" || status=$?
test "$status" -eq 1 || {
    echo 'not ok 1 - over-limit identifiers must fail'
    exit 0
}
cmp "$tmpdir/identifier.expected" "$tmpdir/actual" || {
    echo 'not ok 1 - identifier batches preserve file-local diagnostics'
    exit 0
}
printf '%s\n' src/stdint.c src/missing.c src/empty.c src/limits.c \
    src/fallback.c src/function.c src/bare.c src/unused.c \
    src/excluded.inc.c src/deleted.c > "$CONTROL_INVENTORY"
status=0
PATH="$tmpdir/bin:$PATH" sh \
    "$src_root/tests/_static/sh/staticcheck-c-size-max-header.sh" \
    "$control_root" > "$tmpdir/actual" || status=$?
printf '%s\n' '1..1' \
    '# src/missing.c: uses SIZE_MAX but has no <limits.h>/<stdint.h> include or SIZE_MAX fallback define' \
    'not ok 1 - SIZE_MAX users provide required headers or fallback define' \
    > "$tmpdir/expected"
test "$status" -eq 1 || {
    echo 'not ok 1 - missing SIZE_MAX providers must fail'
    exit 0
}
cmp "$tmpdir/expected" "$tmpdir/actual" || {
    echo 'not ok 1 - SIZE_MAX batches isolate providers and exclude fragments'
    exit 0
}
# Successful providers, no-use skip and empty-inventory skip remain distinct.
for scenario in providers unused empty; do
    case "$scenario" in
        providers)
            printf '%s\n' src/stdint.c src/limits.c src/fallback.c \
                src/function.c src/bare.c > "$CONTROL_INVENTORY"
            verdict='ok 1 - SIZE_MAX users provide required headers or fallback define'
            ;;
        unused)
            printf '%s\n' src/unused.c src/empty.c src/deleted.c > "$CONTROL_INVENTORY"
            verdict='ok 1 # SKIP no SIZE_MAX usage in tracked C source files'
            ;;
        empty)
            printf '%s\n' src/excluded.inc.c > "$CONTROL_INVENTORY"
            verdict='ok 1 # SKIP no tracked C source files'
            ;;
    esac
    PATH="$tmpdir/bin:$PATH" sh \
        "$src_root/tests/_static/sh/staticcheck-c-size-max-header.sh" \
        "$control_root" > "$tmpdir/actual" || {
        echo 'not ok 1 - SIZE_MAX positive or skip control failed'
        exit 0
    }
    printf '%s\n' '1..1' "$verdict" > "$tmpdir/expected"
    cmp "$tmpdir/expected" "$tmpdir/actual" || {
        echo 'not ok 1 - SIZE_MAX positive or skip verdict changed'
        exit 0
    }
done
printf '%s\n' src/valid.c src/empty.c src/deleted.c > "$CONTROL_INVENTORY"
PATH="$tmpdir/bin:$PATH" sh \
    "$src_root/tests/_static/sh/staticcheck-c-identifier-significant-length.sh" \
    "$control_root" > "$tmpdir/actual" || {
    echo 'not ok 1 - identifiers at the limit must pass'
    exit 0
}
printf '%s\n' '1..1' \
    'ok 1 - C identifiers stay within C99 significant-length limit' > "$tmpdir/expected"
cmp "$tmpdir/expected" "$tmpdir/actual" || {
    echo 'not ok 1 - valid identifier verdict changed'
    exit 0
}
# Capture the shell-filtered list before removing a file. This deterministically
# exercises a read failure after discovery, without relying on chmod or races.
REAL_AWK=$(command -v awk)
CONTROL_ROOT=$control_root
CONTROL_LIST="$tmpdir/read-list"
export REAL_AWK CONTROL_ROOT CONTROL_LIST
cat > "$tmpdir/bin/awk" <<'AWK'
#!/bin/sh
while IFS= read -r path; do printf '%s\n' "$path"; done > "$CONTROL_LIST"
rm -f "$CONTROL_ROOT/src/unreadable.c"
exec "$REAL_AWK" "$@" < "$CONTROL_LIST"
AWK
chmod +x "$tmpdir/bin/awk"
for checker in c-identifier-significant-length c-size-max-header; do
    : > "$control_root/src/unreadable.c"
    printf '%s\n' src/unreadable.c > "$CONTROL_INVENTORY"
    status=0
    PATH="$tmpdir/bin:$PATH" sh \
        "$src_root/tests/_static/sh/staticcheck-$checker.sh" \
        "$control_root" > "$tmpdir/actual" || status=$?
    test "$status" -eq 1 || {
        echo 'not ok 1 - an unreadable source must fail rather than skip'
        exit 0
    }
    case "$checker" in
        c-identifier-significant-length)
            verdict='not ok 1 - C identifiers stay within C99 significant-length limit' ;;
        c-size-max-header)
            verdict='not ok 1 - SIZE_MAX users provide required headers or fallback define' ;;
    esac
    printf '%s\n' '1..1' '# src/unreadable.c: unable to read tracked source' \
        "$verdict" > "$tmpdir/expected"
    cmp "$tmpdir/expected" "$tmpdir/actual" || {
        echo 'not ok 1 - read-error diagnostic changed'
        exit 0
    }
done
printf '#!/bin/sh\nexit 2\n' > "$tmpdir/bin/git"
for checker in c-identifier-significant-length c-size-max-header; do
    status=0
    PATH="$tmpdir/bin:$PATH" sh \
        "$src_root/tests/_static/sh/staticcheck-$checker.sh" \
        "$control_root" > "$tmpdir/actual" || status=$?
    test "$status" -eq 1 || {
        echo 'not ok 1 - a failed inventory must fail rather than skip'
        exit 0
    }
    case "$checker" in
        c-identifier-significant-length)
            verdict='not ok 1 - failed to inventory tracked C/C header files' ;;
        c-size-max-header)
            verdict='not ok 1 - failed to inventory tracked C source files' ;;
    esac
    printf '%s\n' '1..1' "$verdict" > "$tmpdir/expected"
    cmp "$tmpdir/expected" "$tmpdir/actual" || {
        echo 'not ok 1 - inventory failure lost its diagnostic'
        exit 0
    }
done
echo 'ok 1 - batched C checks preserve per-file state and reject input failures'
