#!/bin/sh
# Validate build.md Autotools option table against ./configure --help.

set -eu

echo "1..1"

src_root=$1

build_md="$src_root/build.md"
configure_script="$src_root/configure"
configure_ac="$src_root/configure.ac"

if test ! -f "$build_md" || test ! -f "$configure_script" || test ! -f "$configure_ac"; then
    echo "ok 1 # SKIP missing build.md/configure/configure.ac"
    exit 0
fi

tmpdir=`mktemp -d "${TMPDIR:-/tmp}/libsixel-doccheck-XXXXXX"`
cleanup() {
    rm -rf "$tmpdir"
}
trap cleanup EXIT HUP INT TERM

awk '
  /^#### Autotools build options$/ { in_table=1; next }
  in_table && /^#### / { in_table=0 }
  in_table && /^\| `--/ { print }
' "$build_md" \
  | sed -nE 's/^\| `--(enable|disable|with)-([A-Za-z0-9][A-Za-z0-9_-]*)(\[[^`]*\])?(=[^`]*)?`.*/\2/p' \
  | LC_ALL=C sort -u > "$tmpdir/doc_opts.txt"

"$configure_script" --help \
  | sed -nE 's/^[[:space:]]+--(enable|disable|with)-([A-Za-z0-9][A-Za-z0-9_-]*).*/\2/p' \
  | LC_ALL=C sort -u > "$tmpdir/help_opts.txt"

sed -nE 's/^AC_ARG_(ENABLE|WITH)\(\[?([A-Za-z0-9_-]+)\]?.*/\2/p' "$configure_ac" \
  | LC_ALL=C sort -u > "$tmpdir/ac_opts.txt"

LC_ALL=C comm -23 "$tmpdir/ac_opts.txt" "$tmpdir/doc_opts.txt" > "$tmpdir/missing_in_doc.txt"
LC_ALL=C comm -23 "$tmpdir/ac_opts.txt" "$tmpdir/help_opts.txt" > "$tmpdir/missing_in_help.txt"
LC_ALL=C comm -23 "$tmpdir/doc_opts.txt" "$tmpdir/ac_opts.txt" > "$tmpdir/doc_not_in_ac.txt"

if test -s "$tmpdir/missing_in_doc.txt" || test -s "$tmpdir/missing_in_help.txt" || test -s "$tmpdir/doc_not_in_ac.txt"; then
    echo "not ok 1 - build.md and ./configure --help option consistency"
    if test -s "$tmpdir/missing_in_doc.txt"; then
        echo "# Missing in build.md Autotools table:"
        while IFS= read -r opt; do
            test -n "$opt" || continue
            line_no=`grep -nE "AC_ARG_(ENABLE|WITH)\\(\\[?$opt\\]?" "$configure_ac" | sed -n '1s/:.*//p' || true`
            if test -n "$line_no"; then
                echo "#   $opt (defined at configure.ac:$line_no)"
            else
                echo "#   $opt (defined in configure.ac)"
            fi
        done < "$tmpdir/missing_in_doc.txt"
    fi
    if test -s "$tmpdir/missing_in_help.txt"; then
        echo "# Missing in ./configure --help output:"
        while IFS= read -r opt; do
            test -n "$opt" || continue
            line_no=`grep -nE "AC_ARG_(ENABLE|WITH)\\(\\[?$opt\\]?" "$configure_ac" | sed -n '1s/:.*//p' || true`
            if test -n "$line_no"; then
                echo "#   $opt (defined at configure.ac:$line_no)"
            else
                echo "#   $opt (defined in configure.ac)"
            fi
        done < "$tmpdir/missing_in_help.txt"
    fi
    if test -s "$tmpdir/doc_not_in_ac.txt"; then
        echo "# Documented in build.md but not defined by AC_ARG_*:"
        while IFS= read -r opt; do
            test -n "$opt" || continue
            line_no=`grep -nF -- "$opt" "$build_md" | sed -n '1s/:.*//p' || true`
            if test -n "$line_no"; then
                echo "#   $opt (documented at build.md:$line_no)"
            else
                echo "#   $opt (documented in build.md)"
            fi
        done < "$tmpdir/doc_not_in_ac.txt"
    fi
    exit 1
fi

echo "ok 1 - build.md and ./configure --help option consistency"
