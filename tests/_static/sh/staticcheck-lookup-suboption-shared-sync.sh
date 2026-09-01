#!/bin/sh
# Emit TAP for lookup shared_instance suboption schema/docs synchronization.

set -eu

echo "1..1"

src_root=$1
registry_file=$src_root/src/options-registry.c
help_file=$src_root/converters/img2sixel.c
man_file=$src_root/converters/img2sixel.1

if test ! -f "$registry_file" || test ! -f "$help_file" || \
        test ! -f "$man_file"; then
    echo "ok 1 # SKIP missing registry/help/man source file"
    exit 0
fi

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-lookup-shared-sync-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

actual=$tmpdir/actual.tsv
expected=$tmpdir/expected.tsv
missing=$tmpdir/missing.txt
status=0

cat > "$expected" <<'EOT'
SIXEL_LOOKUP_BASE_5BIT	SIXEL_LOOKUP_5BIT_SHARED_INSTANCE
SIXEL_LOOKUP_BASE_6BIT	SIXEL_LOOKUP_6BIT_SHARED_INSTANCE
SIXEL_LOOKUP_BASE_CERTLUT	SIXEL_LOOKUP_CERTLUT_SHARED_INSTANCE
EOT

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
    if (entry ~ /SIXEL_OPTION_SCHEMA_LUT_POLICY/ &&
        entry ~ /"shared_instance"/) {
        count = split(entry, quoted, /"/)
        count = split(entry, fields, /,[[:space:]]*/)
        base = fields[2]
        sub(/^.*\+ /, "", base)
        if (count >= 5) {
            print base "\t" quoted[4]
        }
    }
    entry = ""
}
' "$registry_file" > "$actual"

if ! cmp -s "$expected" "$actual"; then
    echo "# registry: lookup shared_instance mapping mismatch" >> "$missing"
    if command -v diff >/dev/null 2>&1; then
        diff -u "$expected" "$actual" | sed 's/^/# /' >> "$missing"
    fi
    status=1
fi

awk '
BEGIN {
    has_lookup_signature = 0
    has_shared_text = 0
}
index($0, "--lookup-policy=LOOKUPPOLICY") > 0 { has_lookup_signature = 1 }
index($0, "shared_instance") > 0 { has_shared_text = 1 }
END { exit (has_lookup_signature && has_shared_text) ? 0 : 1 }
' "$help_file" || {
    echo "# converters/img2sixel.c: missing lookup shared_instance help" >> "$missing"
    status=1
}

awk '
BEGIN { ok = 0 }
index($0, "shared_instance=") > 0 { ok = 1 }
END { exit ok ? 0 : 1 }
' "$man_file" || {
    echo "# converters/img2sixel.1: missing lookup shared_instance documentation" >> "$missing"
    status=1
}

awk '
BEGIN {
    cert = 0
    bit5 = 0
    bit6 = 0
}
index($0, "SIXEL_LOOKUP_CERTLUT_SHARED_INSTANCE") > 0 { cert = 1 }
index($0, "SIXEL_LOOKUP_5BIT_SHARED_INSTANCE") > 0 { bit5 = 1 }
index($0, "SIXEL_LOOKUP_6BIT_SHARED_INSTANCE") > 0 { bit6 = 1 }
END {
    if (cert && bit5 && bit6) {
        exit 0
    }
    exit 1
}
' "$help_file" || {
    echo "# converters/img2sixel.c: missing lookup shared env var docs" >> "$missing"
    status=1
}

awk '
BEGIN {
    cert = 0
    bit5 = 0
    bit6 = 0
}
index($0, "SIXEL_LOOKUP_CERTLUT_SHARED_INSTANCE") > 0 { cert = 1 }
index($0, "SIXEL_LOOKUP_5BIT_SHARED_INSTANCE") > 0 { bit5 = 1 }
index($0, "SIXEL_LOOKUP_6BIT_SHARED_INSTANCE") > 0 { bit6 = 1 }
END {
    if (cert && bit5 && bit6) {
        exit 0
    }
    exit 1
}
' "$man_file" || {
    echo "# converters/img2sixel.1: missing lookup shared env var docs" >> "$missing"
    status=1
}

if test "$status" -eq 0; then
    echo "ok 1 - lookup shared_instance schema and docs stay in sync"
    exit 0
fi

echo "not ok 1 - lookup shared_instance schema and docs stay in sync"
cat "$missing"
exit 1
