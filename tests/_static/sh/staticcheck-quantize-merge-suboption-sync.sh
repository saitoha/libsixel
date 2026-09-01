#!/bin/sh
# Emit TAP for common quantize merge suboption/env/help consistency.

set -eu

echo "1..1"

src_root=$1
registry_file=$src_root/src/options-registry.c
help_file=$src_root/converters/img2sixel.c

if test ! -f "$registry_file" || test ! -f "$help_file"; then
    echo "ok 1 # SKIP missing registry or converters/img2sixel.c"
    exit 0
fi

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-merge-subopt-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

expected=$tmpdir/expected.tsv
actual=$tmpdir/actual.tsv
help_vars=$tmpdir/help-vars.txt
missing=$tmpdir/missing.txt
status=0

cat > "$expected" <<'EOF'
merge	SIXEL_PALETTE_FINAL_MERGE
merge_lloyd	SIXEL_PALETTE_FINAL_MERGE_ADDITIONAL_LLOYD_ITER_COUNT
merge_oversplit	SIXEL_PALETTE_OVERSPLIT_FACTOR
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
    gsub(/[[:space:]]+/, " ", entry)
    if (entry ~ /SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL, NULL/ &&
        entry ~ /"merge(_oversplit|_lloyd)?"/) {
        split(entry, quoted, /"/)
        print quoted[2] "\t" quoted[4]
    }
    entry = ""
}
' "$registry_file" | LC_ALL=C sort -u > "$actual"

cmp -s "$expected" "$actual" || {
    echo "# registry: common quantize merge mapping mismatch" >> "$missing"
    diff -u "$expected" "$actual" | sed 's/^/# /' >> "$missing" || :
    status=1
}

awk '
/SIXEL_PALETTE_(FINAL_MERGE|OVERSPLIT_FACTOR)/ {
    line = $0
    while (match(line, /SIXEL_PALETTE_[A-Z0-9_]+/)) {
        print substr(line, RSTART, RLENGTH)
        line = substr(line, RSTART + RLENGTH)
    }
}
' "$help_file" | LC_ALL=C sort -u > "$help_vars"

while IFS="$(printf '\t')" read -r key env_name; do
    test -n "$key" || continue
    grep -Fxq "$env_name" "$help_vars" || {
        echo "# converters/img2sixel.c: missing env help entry: $env_name" \
            >> "$missing"
        status=1
    }
done < "$expected"

test "$status" -eq 0 || {
    echo "not ok 1 - quantize suboptions stay in sync"
    cat "$missing"
    exit 1
}

echo "ok 1 - quantize suboptions stay in sync"
exit 0
