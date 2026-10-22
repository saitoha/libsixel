#!/bin/sh
# Emit TAP for the staticcheck suite's own registration contract.
# Policy: docs/testing/staticcheck.md

set -eu

echo "1..1"

src_root=$1
static_root="$src_root/tests/_static/sh"
suite_file="$static_root/staticcheck-suite.sh"
makefile_am="$src_root/tests/Makefile.am"
meson_file="$src_root/tests/meson.build"

test -f "$suite_file" || {
    echo "not ok 1 - staticcheck suite registration is synchronized"
    echo "# missing staticcheck-suite.sh"
    exit 1
}

test -f "$makefile_am" -a -f "$meson_file" || {
    echo "not ok 1 - staticcheck suite registration is synchronized"
    echo "# missing Autotools or Meson test description"
    exit 1
}

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-staticcheck-self-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

expected="$tmpdir/expected"
registered="$tmpdir/registered"
missing="$tmpdir/missing"
unknown="$tmpdir/unknown"

find "$static_root" -maxdepth 1 -type f -name 'staticcheck-*.sh' \
    ! -name 'staticcheck-suite.sh' -exec basename {} \; \
    | sed 's/\.sh$//' | LC_ALL=C sort -u > "$expected"

awk '
{
    rest = $0
    while (match(rest, /staticcheck-[a-z0-9-]+\.sh/)) {
        name = substr(rest, RSTART, RLENGTH)
        sub(/\.sh$/, "", name)
        if (name != "staticcheck-suite") {
            print name
        }
        rest = substr(rest, RSTART + RLENGTH)
    }
}
' "$suite_file" | LC_ALL=C sort -u > "$registered"

comm -23 "$expected" "$registered" > "$missing"
comm -13 "$expected" "$registered" > "$unknown"

test ! -s "$missing" -a ! -s "$unknown" || {
    echo "not ok 1 - staticcheck suite registration is synchronized"
    test ! -s "$missing" || {
        echo "# staticcheck scripts absent from the umbrella suite:"
        sed 's/^/#   /' "$missing"
    }
    test ! -s "$unknown" || {
        echo "# umbrella suite references missing staticcheck scripts:"
        sed 's/^/#   /' "$unknown"
    }
    exit 1
}

awk '
index($0, "staticcheck: staticcheck-clean prepare-staticcheck-headers") {
    target = 1
}
index($0, "prepare-staticcheck-headers:") { headers = 1 }
index($0, "$(MAKE) -C \"$(top_builddir)/include\" sixel.h 6cells.h") {
    refresh = 1
}
index($0, "tests/_static/sh/staticcheck-suite.sh") { runner = 1 }
END { exit target && runner && headers && refresh ? 0 : 1 }
' "$makefile_am" || {
    echo "not ok 1 - staticcheck suite registration is synchronized"
    echo "# tests/Makefile.am must refresh headers before invoking the suite"
    exit 1
}

awk '
index($0, "run_target(\047staticcheck\047") { target = 1 }
index($0, "_static/sh/staticcheck-suite.sh") { runner = 1 }
END { exit target && runner ? 0 : 1 }
' "$meson_file" || {
    echo "not ok 1 - staticcheck suite registration is synchronized"
    echo "# tests/meson.build does not invoke staticcheck-suite.sh"
    exit 1
}

# Source-only TAP contracts register stable roles, not numbered filenames.
# Check both directions so a new marker cannot silently stay unregistered.
awk '
$1 == "run_source_contract" {
    name = $2
    gsub(/"/, "", name)
    print name
}
' "$suite_file" | LC_ALL=C sort > "$tmpdir/source-registered"
find "$src_root/tests" -type f -name '*.t' -exec awk '
/^# Staticcheck:/ { print $3 }
' {} + | LC_ALL=C sort > "$tmpdir/source-declared"
uniq -d "$tmpdir/source-declared" > "$tmpdir/source-duplicates"
comm -3 "$tmpdir/source-declared" "$tmpdir/source-registered" \
    > "$tmpdir/source-drift"
test ! -s "$tmpdir/source-duplicates" -a ! -s "$tmpdir/source-drift" || {
    echo "not ok 1 - staticcheck suite registration is synchronized"
    echo "# duplicate, unregistered, or missing source-contract roles:"
    sed 's/^/# /' "$tmpdir/source-duplicates" "$tmpdir/source-drift"
    exit 1
}

echo "ok 1 - staticcheck suite registration is synchronized"
