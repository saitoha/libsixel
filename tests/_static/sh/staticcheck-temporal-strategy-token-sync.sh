#!/bin/sh
# Emit TAP for temporal strategy token sync across source and tests.

set -eu

echo "1..1"

src_root=$1
source_file=$src_root/src/dither-fixed-8bit.c
temporal_tests_dir=$src_root/tests/processing/dither/temporal

if test ! -f "$source_file" || test ! -d "$temporal_tests_dir"; then
    echo "ok 1 # SKIP missing temporal strategy source or test directory"
    exit 0
fi

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-temporal-token-sync-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

expected_tokens=$tmpdir/expected_tokens.txt
source_tokens=$tmpdir/source_tokens.txt
missing=$tmpdir/missing.txt
status=0

cat > "$expected_tokens" <<'EOF'
diffusion
stbn
stbn-hash
stbn-mask
EOF

awk '
/^sixel_temporal_strategy_token_from_string\(/ {
    in_func = 1
    next
}
in_func && /strcmp\(value, "/ {
    token = $0
    sub(/.*strcmp\(value, "/, "", token)
    sub(/".*/, "", token)
    if (token != "") {
        print token
    }
}
in_func && /^}/ {
    in_func = 0
}
' "$source_file" | LC_ALL=C sort -u > "$source_tokens"

while IFS= read -r token; do
    test -n "$token" || continue
    if ! grep -Fxq "$token" "$source_tokens"; then
        echo "# src/dither-fixed-8bit.c: missing strategy token: $token" \
            >> "$missing"
        status=1
    fi
    if ! find "$temporal_tests_dir" -type f -name '*.t' -exec \
            grep -F "SIXEL_TEMPORAL_STRATEGY=$token" {} + >/dev/null 2>&1; then
        echo "# tests/processing/dither/temporal: missing strategy token use: $token" \
            >> "$missing"
        status=1
    fi
done < "$expected_tokens"

while IFS= read -r token; do
    test -n "$token" || continue
    if ! grep -Fxq "$token" "$expected_tokens"; then
        echo "# src/dither-fixed-8bit.c: unexpected strategy token: $token" \
            >> "$missing"
        status=1
    fi
done < "$source_tokens"

if test "$status" -eq 0; then
    echo "ok 1 - temporal strategy tokens stay synchronized"
    exit 0
fi

echo "not ok 1 - temporal strategy tokens stay synchronized"
cat "$missing"
exit 1
