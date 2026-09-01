#!/bin/sh
# Emit TAP for kmeans suboption to env-var mapping consistency.

set -eu

echo "1..1"

src_root=$1
registry_file=$src_root/src/options-registry.c
help_file=$src_root/converters/img2sixel.c

if test ! -f "$registry_file" || test ! -f "$help_file"; then
    echo "ok 1 # SKIP missing registry or converters/img2sixel.c"
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
prune	SIXEL_PALETTE_KMEANS_PRUNE
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
/SIXEL_REGISTRY_(CHOICE|FREE)\(/ {
    in_block = 1
    entry = $0
    next
}
in_block {
    entry = entry " " $0
}
in_block && /\),[[:space:]]*$/ {
    in_block = 0
    if (entry ~ /SIXEL_QUANTIZE_BASE_KMEANS/) {
        count = split(entry, quoted, /"/)
        if (count >= 4 && quoted[4] ~ /^SIXEL_PALETTE_KMEANS_/) {
            printf "%s\t%s\n", quoted[2], quoted[4]
        }
    }
    entry = ""
}
' "$registry_file" | LC_ALL=C sort -u > "$actual"

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
        echo "# registry: missing kmeans key/env pair: $key -> $env" \
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
        echo "# registry: unexpected kmeans key/env pair: $key -> $env" \
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
