#!/bin/sh
# Validate build.md Meson option table against meson_options.txt.

set -eu

echo "1..1"

src_root=$1

build_md="$src_root/build.md"
meson_opts="$src_root/meson_options.txt"

if test ! -f "$build_md" || test ! -f "$meson_opts"; then
    echo "ok 1 # SKIP missing build.md/meson_options.txt"
    exit 0
fi

tmpdir=`mktemp -d "${TMPDIR:-/tmp}/libsixel-doccheck-XXXXXX"`
cleanup() {
    rm -rf "$tmpdir"
}
trap cleanup EXIT HUP INT TERM

awk '
  /^#### Meson build options$/ { in_table=1; next }
  in_table && /^#### / { in_table=0 }
  in_table && /^\| `-D/ { print }
' "$build_md" \
  | sed -nE 's/^\| `-D([A-Za-z0-9_]+)=?[^`]*`.*/\1/p' \
  | LC_ALL=C sort -u > "$tmpdir/doc_opts.txt"

sed -nE "s/^option\\('([A-Za-z0-9_]+)'.*/\\1/p" "$meson_opts" \
  | LC_ALL=C sort -u > "$tmpdir/meson_opts.txt"

printf '%s\n' b_lto b_lto_mode b_pgo b_sanitize \
  | LC_ALL=C sort -u > "$tmpdir/doc_extra_allowlist.txt"

LC_ALL=C comm -23 "$tmpdir/meson_opts.txt" "$tmpdir/doc_opts.txt" > "$tmpdir/missing_in_doc.txt"
LC_ALL=C comm -23 "$tmpdir/doc_opts.txt" "$tmpdir/meson_opts.txt" > "$tmpdir/doc_not_in_meson_raw.txt"
grep -vxFf "$tmpdir/doc_extra_allowlist.txt" "$tmpdir/doc_not_in_meson_raw.txt" > "$tmpdir/doc_not_in_meson.txt" || true

if test -s "$tmpdir/missing_in_doc.txt" || test -s "$tmpdir/doc_not_in_meson.txt"; then
    echo "not ok 1 - build.md and meson_options.txt option consistency"
    if test -s "$tmpdir/missing_in_doc.txt"; then
        echo "# Missing in build.md Meson table:"
        sed 's/^/#   /' "$tmpdir/missing_in_doc.txt"
    fi
    if test -s "$tmpdir/doc_not_in_meson.txt"; then
        echo "# Documented in build.md but not defined in meson_options.txt:"
        sed 's/^/#   /' "$tmpdir/doc_not_in_meson.txt"
    fi
    exit 1
fi

echo "ok 1 - build.md and meson_options.txt option consistency"
