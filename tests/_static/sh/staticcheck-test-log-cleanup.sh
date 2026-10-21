#!/bin/sh
# Exercise the cleanup's ownership, batching and failure boundary.
# Policy: docs/build/test-harness.md
set -eux
echo '1..1'
set -v

src_root=$1
tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-log-cleanup-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM
mkdir "$tmpdir/logs" "$tmpdir/bin" "$tmpdir/logs/directory.log"
real_rm=$(command -v rm)
export real_rm
cleanup_calls="$tmpdir/calls"
export cleanup_calls
cat > "$tmpdir/bin/rm" <<'EOF'
#!/bin/sh
printf '.\n' >> "$cleanup_calls"
test "${cleanup_fail-no}" != yes || exit 1
# Simulate several RMS versions of one filename without needing GNV.
test "${cleanup_versions-no}" != yes || {
    test "$(wc -l < "$cleanup_calls")" -ge 3 || exit 0
}
exec "$real_rm" "$@"
EOF
chmod +x "$tmpdir/bin/rm"
# Enough files to detect accidental one-process-per-file deletion, but no
# host-specific maximum batch size is assumed.
i=0
while test "$i" -lt 100; do
    : > "$tmpdir/logs/result $i.log"
    : > "$tmpdir/logs/result $i.trs"
    i=$((i + 1))
done
: > "$tmpdir/logs/staticcheck-saved.log"
: > "$tmpdir/logs/staticcheck-saved.trs"
: > "$tmpdir/logs/source.t"
PATH="$tmpdir/bin:$PATH" sh "$src_root/build-aux/clean-test-logs.sh" \
    "$tmpdir/logs" darwin || {
    echo 'not ok 1 - runtime report cleanup failed'
    exit 0
}
test "$(wc -l < "$cleanup_calls")" -lt 10 &&
test "$(find "$tmpdir/logs" -type f | wc -l)" -eq 3 &&
test -f "$tmpdir/logs/staticcheck-saved.log" &&
test -f "$tmpdir/logs/staticcheck-saved.trs" &&
test -f "$tmpdir/logs/source.t" &&
test -d "$tmpdir/logs/directory.log" || {
    echo 'not ok 1 - cleanup must batch runtime files and preserve other owners'
    exit 0
}
: > "$tmpdir/logs/failure.trs"
PATH="$tmpdir/bin:$PATH" cleanup_fail=yes \
    sh "$src_root/build-aux/clean-test-logs.sh" "$tmpdir/logs" darwin && {
    echo 'not ok 1 - removal failure was hidden'
    exit 0
}
sh "$src_root/build-aux/clean-test-logs.sh" "$tmpdir/missing" darwin && {
    echo 'not ok 1 - discovery failure was hidden'
    exit 0
}
: > "$cleanup_calls"
PATH="$tmpdir/bin:$PATH" cleanup_versions=yes \
    sh "$src_root/build-aux/clean-test-logs.sh" "$tmpdir/logs" openvms || {
    echo 'not ok 1 - versioned report cleanup failed'
    exit 0
}
test ! -e "$tmpdir/logs/failure.trs" &&
test "$(wc -l < "$cleanup_calls")" -eq 3 || {
    echo 'not ok 1 - cleanup left an older report version behind'
    exit 0
}
echo 'ok 1 - runtime report cleanup preserves ownership and propagates failures'
exit 0
