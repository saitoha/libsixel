#!/bin/sh
# Consume Automake's complete file inventory without expanding it into argv.
set -eu
srcdir=$1
topsrcdir=$2
topbuilddir=$3
distdir=$4
while IFS= read -r file; do
    case "$file" in
        "$srcdir"/*) file=${file#"$srcdir"/} ;;
        "$topsrcdir"/*) file=$topbuilddir/${file#"$topsrcdir"/} ;;
    esac
    test -n "$file" || continue
    source_dir=$srcdir
    if test -f "$file" || test -d "$file"; then
        source_dir=.
    fi
    destination=$distdir/$file
    parent=${destination%/*}
    test -d "$parent" || mkdir -p "$parent"
    if test -d "$source_dir/$file"; then
        # VPATH distributions merge source and build trees, as Automake does.
        if test -d "$srcdir/$file" && test "$source_dir" != "$srcdir"; then
            cp -fpR "$srcdir/$file" "$parent"
        fi
        cp -fpR "$source_dir/$file" "$parent"
    elif test ! -f "$destination"; then
        cp -p "$source_dir/$file" "$destination"
    fi
done
