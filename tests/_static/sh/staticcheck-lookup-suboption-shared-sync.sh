#!/bin/sh
# Emit TAP for lookup shared_instance suboption schema/docs synchronization.

set -eu

echo "1..1"

src_root=$1
encoder_file=$src_root/src/encoder.c
help_file=$src_root/converters/img2sixel.c
man_file=$src_root/converters/img2sixel.1

if test ! -f "$encoder_file" || test ! -f "$help_file" || test ! -f "$man_file"; then
    echo "ok 1 # SKIP missing encoder/help/man source file"
    exit 0
fi

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-lookup-shared-sync-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

actual=$tmpdir/actual.tsv
expected=$tmpdir/expected.tsv
missing=$tmpdir/missing.txt
status=0

cat > "$expected" <<'EOT'
auto	NULL
5bit	g_subkeys_lookup_policy_shared
6bit	g_subkeys_lookup_policy_shared
none	NULL
certlut	g_subkeys_lookup_policy_shared
eytzinger	NULL
fhedt	NULL
vptree	NULL
rbc	NULL
mahalanobis	NULL
EOT

awk '
/g_schema_lookup_policy_values\[\][[:space:]]*=[[:space:]]*\{/ {
    in_block = 1
    in_entry = 0
    field = 0
    name = ""
    subkeys = ""
    next
}
in_block && /^[[:space:]]*};/ {
    in_block = 0
    next
}
!in_block {
    next
}
{
    line = $0
    if (line ~ /^[[:space:]]*\{[[:space:]]*$/) {
        in_entry = 1
        field = 0
        name = ""
        subkeys = ""
        next
    }
    if (!in_entry) {
        next
    }
    if (line ~ /^[[:space:]]*\},?[[:space:]]*$/) {
        if (name != "" && subkeys != "") {
            printf "%s\t%s\n", name, subkeys
        }
        in_entry = 0
        next
    }
    if (line ~ /^[[:space:]]*"[^"]+",[[:space:]]*$/) {
        token = line
        sub(/^[[:space:]]*"/, "", token)
        sub(/",[[:space:]]*$/, "", token)
        field += 1
        if (field == 1) {
            name = token
        }
        next
    }
    if (line ~ /^[[:space:]]*[A-Za-z0-9_]+,[[:space:]]*$/) {
        token = line
        sub(/^[[:space:]]*/, "", token)
        sub(/,[[:space:]]*$/, "", token)
        field += 1
        if (field == 3) {
            subkeys = token
        }
    }
}
' "$encoder_file" > "$actual"

if ! cmp -s "$expected" "$actual"; then
    echo "# src/encoder.c: lookup policy schema mismatch" >> "$missing"
    if command -v diff >/dev/null 2>&1; then
        diff -u "$expected" "$actual" | sed 's/^/# /' >> "$missing"
    fi
    status=1
fi

awk '
BEGIN { ok = 0 }
index($0, "--lookup-policy=LOOKUPPOLICY[:shared_instance=0|1]") > 0 { ok = 1 }
END { exit ok ? 0 : 1 }
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
