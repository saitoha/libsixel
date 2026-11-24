#!/bin/sh
# Enforce the source spelling for a line break before each shell TAP result.
# Policy: docs/testing/guide.md
set -eu
src_root=$1
export LC_ALL=C
tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-tap-newline-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM
check_sources() {
    awk -f "$src_root/tests/_static/awk/shell-words.awk" \
        -f "$src_root/tests/_static/awk/tap-result-newline.awk" "$1"
}

# Exercise success, failure and skip spellings, including inline assertions
# and line continuations. Quoted sample source and heredocs are inert.
cat > "$tmpdir/control.t" <<'SOURCE'
echo "1..1"
printf '\n%s\n' "ok 1 - success"
printf "\n%s\n" "not ok 1 - failure"
printf '\nok 1 # SKIP no backend\n'
: || { printf '\n%s\n' 'not ok 1 - inline'; }
printf '%s\n' 'echo "ok 1 - quoted source"'
echo printf "ok 1 - this is an argument to echo"
cat <<'LITERAL'
echo "not ok 1 - fixture payload"
LITERAL
SOURCE
printf '%s\n' "$tmpdir/control.t" > "$tmpdir/paths"
check_sources "$tmpdir/paths" || exit 1

cat >> "$tmpdir/control.t" <<'SOURCE'
echo 'ok 1 - success'
: || { echo "not ok" 1 - 'inline failure'; }
printf 'ok 1 # SKIP no backend\n'
printf '%s\n' \
    'not ok 1 - continued'
echo not ok 1 - unquoted
printf 'ok\n'
SOURCE
if check_sources "$tmpdir/paths" > "$tmpdir/diagnostics"; then
    echo 'TAP result newline check accepted the negative controls' >&2
    exit 1
fi
printf '%s\n' \
    "$tmpdir/control.t:11: TAP result must start with a newline" \
    "$tmpdir/control.t:12: TAP result must start with a newline" \
    "$tmpdir/control.t:13: TAP result must start with a newline" \
    "$tmpdir/control.t:14: TAP result must start with a newline" \
    "$tmpdir/control.t:16: TAP result must start with a newline" \
    "$tmpdir/control.t:17: TAP result must start with a newline" \
    > "$tmpdir/expected"
diff -u "$tmpdir/expected" "$tmpdir/diagnostics" || exit 1

sh "$src_root/tests/_static/sh/list-source-files.sh" "$src_root/tests" \
    --name '*.t' > "$tmpdir/paths"
check_sources "$tmpdir/paths"
