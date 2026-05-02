#!/bin/sh
# Emit TAP for unified factory class-id annotation and gperf sync checks.

set -eu

src_root=$1
generator=$src_root/tools/gen_factory_classid_gperf.awk
gperf_file=$src_root/src/classid-factory.gperf
idl_file=$src_root/include/6cells.idl

echo "1..2"

if test ! -f "$generator"; then
    echo "not ok 1 - factory classid registry stays in sync"
    echo "# missing generator: $generator"
    echo "not ok 2 - coclass classids resolve to factory constructors"
    echo "# missing generator: $generator"
    exit 1
fi

if test ! -f "$gperf_file"; then
    echo "not ok 1 - factory classid registry stays in sync"
    echo "# missing gperf file: $gperf_file"
    echo "not ok 2 - coclass classids resolve to factory constructors"
    echo "# missing gperf file: $gperf_file"
    exit 1
fi

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-factory-classid-sync-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

expected=$tmpdir/classid-factory.expected.gperf
failed=0

if ! awk -f "$generator" \
    "$src_root"/src/lookup-policy-5bit.h \
    "$src_root"/src/lookup-policy-6bit.h \
    "$src_root"/src/lookup-policy-certlut.h \
    "$src_root"/src/lookup-policy-eytzinger.h \
    "$src_root"/src/lookup-policy-fhedt.h \
    "$src_root"/src/lookup-policy-mahalanobis.h \
    "$src_root"/src/lookup-policy-mono-darkbg.h \
    "$src_root"/src/lookup-policy-mono-lightbg.h \
    "$src_root"/src/lookup-policy-none.h \
    "$src_root"/src/lookup-policy-rbc.h \
    "$src_root"/src/lookup-policy-vptree.h \
    "$src_root"/src/dither-policy-none.h \
    "$src_root"/src/dither-policy-fs.h \
    "$src_root"/src/dither-policy-atkinson.h \
    "$src_root"/src/dither-policy-jajuni.h \
    "$src_root"/src/dither-policy-stucki.h \
    "$src_root"/src/dither-policy-burkes.h \
    "$src_root"/src/dither-policy-sierra1.h \
    "$src_root"/src/dither-policy-sierra2.h \
    "$src_root"/src/dither-policy-sierra3.h \
    "$src_root"/src/dither-policy-lso2.h \
    "$src_root"/src/dither-policy-a-dither.h \
    "$src_root"/src/dither-policy-x-dither.h \
    "$src_root"/src/dither-policy-bluenoise.h \
    "$src_root"/src/dither-policy-interframe.h \
    "$src_root"/src/loader-builtin.h \
    "$src_root"/src/loader-coregraphics.h \
    "$src_root"/src/loader-gd.h \
    "$src_root"/src/loader-gdk-pixbuf2.h \
    "$src_root"/src/loader-gnome-thumbnailer.h \
    "$src_root"/src/loader-libjpeg.h \
    "$src_root"/src/loader-libpng.h \
    "$src_root"/src/loader-librsvg.h \
    "$src_root"/src/loader-libtiff.h \
    "$src_root"/src/loader-libwebp.h \
    "$src_root"/src/loader-quicklook.h \
    "$src_root"/src/loader-wic.h \
	    "$src_root"/src/output.h \
	    "$src_root"/src/chunk.h \
	    "$src_root"/src/frame.h \
	    "$src_root"/src/palette.h \
	    "$src_root"/src/loader-manager.h \
    "$src_root"/src/timeline-logger.h \
    >"$expected"; then
    echo "not ok 1 - factory classid registry stays in sync"
    echo "# failed to regenerate classid-factory.gperf"
    failed=1
elif cmp -s "$gperf_file" "$expected"; then
    echo "ok 1 - factory classid registry stays in sync"
else
    echo "not ok 1 - factory classid registry stays in sync"
    if command -v diff >/dev/null 2>&1; then
        diff -u "$gperf_file" "$expected" | sed 's/^/# /'
    else
        echo "# diff not found"
    fi
    failed=1
fi

if test ! -f "$idl_file"; then
    echo "not ok 2 - coclass classids resolve to factory constructors"
    echo "# missing IDL file: $idl_file"
    exit 1
fi

if awk -v idl_file="$idl_file" -v gperf_file="$gperf_file" '
function trim(text) {
    gsub(/^[ \t]+/, "", text)
    gsub(/[ \t]+$/, "", text)
    return text
}
FILENAME == idl_file {
    line = trim($0)
    if (line ~ /^\[classid\("[A-Za-z0-9_.\/-]+"\)\]$/) {
        pending_classid = line
        sub(/^\[classid\("/, "", pending_classid)
        sub(/"\)\]$/, "", pending_classid)
        next
    }
    if (line ~ /^\[serviceid\("[A-Za-z0-9_.\/-]+"\)\]$/) {
        pending_serviceid = line
        sub(/^\[serviceid\("/, "", pending_serviceid)
        sub(/"\)\]$/, "", pending_serviceid)
        next
    }
    if (line ~ /^coclass[ \t]+[A-Za-z_][A-Za-z0-9_]*[ \t]*\{$/) {
        coclass = line
        sub(/^coclass[ \t]+/, "", coclass)
        sub(/[ \t]*\{$/, "", coclass)
        if (pending_serviceid != "") {
            pending_serviceid = ""
        } else if (pending_classid == "") {
            print "missing classid before coclass: " coclass
        } else {
            if (pending_classid in coclass_by_classid) {
                print "duplicate coclass classid: " pending_classid
            } else {
                coclass_by_classid[pending_classid] = coclass
                classids[++classid_count] = pending_classid
            }
        }
        pending_classid = ""
        pending_serviceid = ""
        next
    }
    if (line != "" &&
        line !~ /^\/\// &&
        line !~ /^\/\*/ &&
        line !~ /^\*/ &&
        line !~ /^\[[^]]+\]$/) {
        pending_classid = ""
        pending_serviceid = ""
    }
    next
}
FILENAME == gperf_file {
    line = trim($0)
    if (line ~ /^# define[ \t]+[A-Za-z0-9_]+[ \t]+[A-Za-z_][A-Za-z0-9_]*$/) {
        macro = line
        sub(/^# define[ \t]+/, "", macro)
        sub(/[ \t]+[A-Za-z_][A-Za-z0-9_]*$/, "", macro)
        constructor = line
        sub(/^# define[ \t]+[A-Za-z0-9_]+[ \t]+/, "", constructor)
        constructors[macro] = constructor
        next
    }
    if (line ~ /^[A-Za-z0-9_.\/-]+,[ \t]*[A-Za-z0-9_]+$/) {
        classid = line
        sub(/,[ \t]*[A-Za-z0-9_]+$/, "", classid)
        macro = line
        sub(/^[A-Za-z0-9_.\/-]+,[ \t]*/, "", macro)
        registry_macro[classid] = macro
    }
}
END {
    if (classid_count == 0) {
        print "missing coclass classid definitions"
    }
    for (i = 1; i <= classid_count; ++i) {
        classid = classids[i]
        if (!(classid in registry_macro)) {
            print "missing factory registry entry: " classid \
                " (" coclass_by_classid[classid] ")"
            continue
        }
        macro = registry_macro[classid]
        if (!(macro in constructors)) {
            print "missing nonzero factory constructor: " classid \
                " -> " macro
        }
    }
}
' "$idl_file" "$gperf_file" >"$tmpdir/coclass-factory.txt"; then
    if test -s "$tmpdir/coclass-factory.txt"; then
        echo "not ok 2 - coclass classids resolve to factory constructors"
        sed 's/^/# /' "$tmpdir/coclass-factory.txt"
        failed=1
    else
        echo "ok 2 - coclass classids resolve to factory constructors"
    fi
else
    echo "not ok 2 - coclass classids resolve to factory constructors"
    sed 's/^/# /' "$tmpdir/coclass-factory.txt"
    failed=1
fi

exit "$failed"
