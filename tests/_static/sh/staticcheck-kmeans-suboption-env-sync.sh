#!/bin/sh
# Emit TAP for kmeans suboption to env-var mapping consistency.

set -eu

echo "1..1"

src_root=$1
encoder_file=$src_root/src/encoder.c
help_file=$src_root/converters/img2sixel.c

if test ! -f "$encoder_file" || test ! -f "$help_file"; then
    echo "ok 1 # SKIP missing src/encoder.c or converters/img2sixel.c"
    exit 0
fi

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-kmeans-env-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

expected=$tmpdir/expected.tsv
actual=$tmpdir/actual.tsv
help_vars=$tmpdir/help_vars.txt
missing=$tmpdir/missing.txt

cat > "$expected" <<'EOF'
inittype	SIXEL_PALETTE_KMEANS_INITTYPE
threshold	SIXEL_PALETTE_KMEANS_THRESHOLD
binning	SIXEL_PALETTE_KMEANS_BINNING
binbits	SIXEL_PALETTE_KMEANS_BINBITS
mapping	SIXEL_PALETTE_KMEANS_MAPPING
softdist	SIXEL_PALETTE_KMEANS_SOFTDIST
autoratio	SIXEL_PALETTE_KMEANS_AUTORATIO
feedback	SIXEL_PALETTE_KMEANS_FEEDBACK
seed	SIXEL_PALETTE_KMEANS_SEED
restarts	SIXEL_PALETTE_KMEANS_RESTARTS
iter	SIXEL_PALETTE_KMEANS_ITER
iter_max	SIXEL_PALETTE_KMEANS_ITER_COUNT_MAX
miniter	SIXEL_PALETTE_KMEANS_MINITER
polish_iter	SIXEL_PALETTE_KMEANS_POLISH_ITER
feedback_slots	SIXEL_PALETTE_KMEANS_FEEDBACK_SLOTS
feedback_interval	SIXEL_PALETTE_KMEANS_FEEDBACK_INTERVAL
EOF

awk '
/g_subkeys_quantize_model_kmeans\[\][[:space:]]*=[[:space:]]*\{/ {
    in_block = 1
    key = ""
    field_index = 0
    next
}
in_block && /^[[:space:]]*};/ {
    in_block = 0
    next
}
!in_block { next }
{
    line = $0
    if (line ~ /^[[:space:]]*\{[[:space:]]*$/) {
        key = ""
        field_index = 0
        next
    }
    if (line ~ /^[[:space:]]*"[^"]+",[[:space:]]*$/) {
        token = line
        sub(/^[[:space:]]*"/, "", token)
        sub(/",.*/, "", token)
        field_index += 1
        if (field_index == 1) {
            key = token
            next
        }
        if (field_index == 3) {
            if (key != "") {
                if (token ~ /^SIXEL_PALETTE_KMEANS_[A-Z0-9_]+$/) {
                    printf "%s\t%s\n", key, token
                    key = ""
                    next
                }
            }
        }
    } else if (line ~ /^[[:space:]]*NULL,[[:space:]]*$/) {
        field_index += 1
        if (field_index == 3) {
            key = ""
        }
    }
}
' "$encoder_file" | LC_ALL=C sort -u > "$actual"

awk '
/^[[:space:]]*"SIXEL_PALETTE_KMEANS_[A-Z0-9_]+"/ {
    line = $0
    sub(/^[[:space:]]*"/, "", line)
    sub(/".*$/, "", line)
    print line
}
' "$help_file" | LC_ALL=C sort -u > "$help_vars"

status=0

while IFS="$(printf '\t')" read -r key env; do
    test -n "$key" || continue
    if ! grep -Fxq "$key	$env" "$actual"; then
        echo "# src/encoder.c: missing kmeans key/env pair: $key -> $env" \
            >> "$missing"
        status=1
    fi
    if ! grep -Fxq "$env" "$help_vars"; then
        echo "# converters/img2sixel.c: missing env help entry: $env" \
            >> "$missing"
        status=1
    fi
done < "$expected"

while IFS="$(printf '\t')" read -r key env; do
    test -n "$key" || continue
    if ! grep -Fxq "$key	$env" "$expected"; then
        echo "# src/encoder.c: unexpected kmeans key/env pair: $key -> $env" \
            >> "$missing"
        status=1
    fi
done < "$actual"

if test "$status" -eq 0; then
    echo "ok 1 - kmeans suboptions and env vars stay in sync"
    exit 0
fi

echo "not ok 1 - kmeans suboptions and env vars stay in sync"
cat "$missing"
exit 1
