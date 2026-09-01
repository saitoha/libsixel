#!/bin/sh
# Emit TAP for kmedoids suboption to env-var mapping consistency.

set -eu

echo "1..1"

src_root=$1
registry_file=$src_root/src/options-registry.c
help_file=$src_root/converters/img2sixel.c

if test ! -f "$registry_file" || test ! -f "$help_file"; then
    echo "ok 1 # SKIP missing registry or converters/img2sixel.c"
    exit 0
fi

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-kmedoids-env-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

expected=$tmpdir/expected.tsv
actual=$tmpdir/actual.tsv
help_vars=$tmpdir/help_vars.txt
missing=$tmpdir/missing.txt

cat > "$expected" <<'EOF'
algo	SIXEL_PALETTE_KMEDOIDS_ALGO
seed	SIXEL_PALETTE_KMEDOIDS_SEED
iter	SIXEL_PALETTE_KMEDOIDS_ITER
sample	SIXEL_PALETTE_KMEDOIDS_SAMPLE
clara_trials	SIXEL_PALETTE_KMEDOIDS_CLARA_TRIALS
clara_sample	SIXEL_PALETTE_KMEDOIDS_CLARA_SAMPLE
clarans_local	SIXEL_PALETTE_KMEDOIDS_CLARANS_LOCAL
clarans_neighbors	SIXEL_PALETTE_KMEDOIDS_CLARANS_NEIGHBORS
bandit_iter	SIXEL_PALETTE_KMEDOIDS_BANDIT_ITER
bandit_candidates	SIXEL_PALETTE_KMEDOIDS_BANDIT_CANDIDATES
bandit_batch	SIXEL_PALETTE_KMEDOIDS_BANDIT_BATCH
histbits	SIXEL_PALETTE_KMEDOIDS_HISTBITS
point_budget	SIXEL_PALETTE_KMEDOIDS_POINT_BUDGET
rare_keep	SIXEL_PALETTE_KMEDOIDS_RARE_KEEP
prune_mass	SIXEL_PALETTE_KMEDOIDS_PRUNE_MASS
auction	SIXEL_PALETTE_KMEDOIDS_AUCTION
auction_shortlist	SIXEL_PALETTE_KMEDOIDS_AUCTION_SHORTLIST
EOF

awk '
/SIXEL_REGISTRY_[A-Z_]+\(/ {
    in_block = 1
    entry = $0
    next
}
in_block {
    entry = entry " " $0
}
in_block && /\),[[:space:]]*$/ {
    in_block = 0
    if (entry ~ /SIXEL_QUANTIZE_BASE_MEDOIDS/) {
        count = split(entry, quoted, /"/)
        if (count >= 4 && quoted[4] ~ /^SIXEL_PALETTE_KMEDOIDS_/) {
            printf "%s\t%s\n", quoted[2], quoted[4]
        }
    }
    entry = ""
}
' "$registry_file" | LC_ALL=C sort -u > "$actual"

awk '
/^[[:space:]]*"SIXEL_PALETTE_KMEDOIDS_[A-Z0-9_]+"/ {
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
        echo "# registry: missing kmedoids key/env pair: $key -> $env" \
            >> "$missing"
        status=1
    fi
    if ! grep -Fxq "$env" "$help_vars"; then
        echo "# converters/img2sixel.c: missing env help entry: $env" \
            >> "$missing"
        status=1
    fi
done < "$expected"

if test "$status" -eq 0; then
    echo "ok 1 - kmedoids suboptions and env vars stay in sync"
    exit 0
fi

echo "not ok 1 - kmedoids suboptions and env vars stay in sync"
cat "$missing"
exit 1
