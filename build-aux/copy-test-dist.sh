#!/bin/sh
# Consume Automake's complete file inventory without expanding it into argv.
set -eu
srcdir=$1
topsrcdir=$2
topbuilddir=$3
distdir=$4
source_only=${5:-}
batch_dir=
batch_names='
'
set --

copy_batch() {
    test "$#" -eq 0 || cp -fp "$@" "$batch_dir"
}

while IFS= read -r file; do
    case "$file" in
        "$srcdir"/*) file=${file#"$srcdir"/} ;;
        "$topsrcdir"/*) file=$topbuilddir/${file#"$topsrcdir"/} ;;
    esac
    test -n "$file" || continue
    source_dir=$srcdir
    if test "$source_only" != source-only &&
        { test -f "$file" || test -d "$file"; }; then
        source_dir=.
    fi
    destination=$distdir/$file
    parent=${destination%/*}
    test -d "$parent" || mkdir -p "$parent"
    if test -d "$source_dir/$file"; then
        copy_batch "$@"
        set --
        batch_names='
'
        # VPATH distributions merge source and build trees, as Automake does.
        if test -d "$srcdir/$file" && test "$source_dir" != "$srcdir"; then
            cp -fpR "$srcdir/$file" "$parent"
            chmod -R u+w "$destination"
        fi
        cp -fpR "$source_dir/$file" "$parent"
        # distcheck sources are read-only. Later overlapping directory
        # entries must be able to merge into this distribution copy.
        chmod -R u+w "$destination"
    elif test "$source_only" = source-only || test ! -f "$destination"; then
        # Keep argv below Windows shell limits and preserve manifest order.
        if test "$parent" != "$batch_dir" || test "$#" -eq 48; then
            copy_batch "$@"
            set --
            batch_dir=$parent
            batch_names='
'
        fi
        name=${destination##*/}
        case "$batch_names" in
            *"
$name
"*) continue ;;
        esac
        batch_names="$batch_names$name
"
        set -- "$@" "$source_dir/$file"
    fi
done
copy_batch "$@"
