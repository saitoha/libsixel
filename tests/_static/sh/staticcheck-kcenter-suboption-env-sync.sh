#!/bin/sh
# Emit TAP for kcenter suboption to env-var mapping consistency.

set -eu

echo "1..1"

src_root=$1
encoder_file=$src_root/src/encoder.c
help_file=$src_root/converters/img2sixel.c

if test ! -f "$encoder_file" || test ! -f "$help_file"; then
    echo "ok 1 # SKIP missing src/encoder.c or converters/img2sixel.c"
    exit 0
fi

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-kcenter-env-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

expected=$tmpdir/expected.tsv
actual=$tmpdir/actual.tsv
help_vars=$tmpdir/help_vars.txt
missing=$tmpdir/missing.txt

cat > "$expected" <<'TSV'
algo	SIXEL_PALETTE_KCENTER_ALGO
profile	SIXEL_PALETTE_KCENTER_PROFILE
seed	SIXEL_PALETTE_KCENTER_SEED
auto_policy	SIXEL_PALETTE_KCENTER_AUTO_POLICY
auto_fft_threshold	SIXEL_PALETTE_KCENTER_AUTO_FFT_THRESHOLD
candidate_policy	SIXEL_PALETTE_KCENTER_CANDIDATE_POLICY
restarts	SIXEL_PALETTE_KCENTER_RESTARTS
iter	SIXEL_PALETTE_KCENTER_ITER
histbits	SIXEL_PALETTE_KCENTER_HISTBITS
point_budget	SIXEL_PALETTE_KCENTER_POINT_BUDGET
rare_keep	SIXEL_PALETTE_KCENTER_RARE_KEEP
prune_mass	SIXEL_PALETTE_KCENTER_PRUNE_MASS
budget_policy	SIXEL_PALETTE_KCENTER_BUDGET_POLICY
budget_scale	SIXEL_PALETTE_KCENTER_BUDGET_SCALE
swap_topk	SIXEL_PALETTE_KCENTER_SWAP_TOPK
swap_update	SIXEL_PALETTE_KCENTER_SWAP_UPDATE
swap_patience	SIXEL_PALETTE_KCENTER_SWAP_PATIENCE
TSV

awk '
/g_subkeys_quantize_model_center\[\][[:space:]]*=[[:space:]]*\{/ {
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
                if (token ~ /^SIXEL_PALETTE_KCENTER_[A-Z0-9_]+$/) {
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
/^[[:space:]]*"SIXEL_PALETTE_KCENTER_[A-Z0-9_]+"/ {
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
        echo "# src/encoder.c: missing kcenter key/env pair: $key -> $env" \
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
        echo "# src/encoder.c: unexpected kcenter key/env pair: $key -> $env" \
            >> "$missing"
        status=1
    fi
done < "$actual"

if test "$status" -eq 0; then
    echo "ok 1 - kcenter suboptions and env vars stay in sync"
    exit 0
fi

echo "not ok 1 - kcenter suboptions and env vars stay in sync"
cat "$missing"
exit 1
