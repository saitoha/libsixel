#!/bin/sh
# Emit TAP for legacy output and sixel emitter boundary checks.

set -eu

src_root=${1:-}

echo "1..5"

if test -z "$src_root"; then
    echo "not ok 1 - src emitter construction uses factory"
    echo "# src_root argument is required"
    echo "not ok 2 - legacy public output API stays exported"
    echo "# src_root argument is required"
    echo "not ok 3 - 6cells IDL uses sixel_emitter as canonical name"
    echo "# src_root argument is required"
    echo "not ok 4 - emitter classid and legacy alias are registered"
    echo "# src_root argument is required"
    echo "not ok 5 - src vtbl callers use sixel_emitter names"
    echo "# src_root argument is required"
    exit 1
fi

if test ! -d "$src_root/src"; then
    echo "not ok 1 - src emitter construction uses factory"
    echo "# missing source directory: $src_root/src"
    echo "not ok 2 - legacy public output API stays exported"
    echo "# missing source directory: $src_root/src"
    echo "not ok 3 - 6cells IDL uses sixel_emitter as canonical name"
    echo "# missing source directory: $src_root/src"
    echo "not ok 4 - emitter classid and legacy alias are registered"
    echo "# missing source directory: $src_root/src"
    echo "not ok 5 - src vtbl callers use sixel_emitter names"
    echo "# missing source directory: $src_root/src"
    exit 1
fi

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-emitter-boundary-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

idl_file=$src_root/include/6cells.idl
public_header=$src_root/include/sixel.h.in
classid_gperf=$src_root/src/classid-factory.gperf
violations=$tmpdir/direct-output-new.txt
legacy_vtbl=$tmpdir/legacy-vtbl.txt
failed=0

find "$src_root/src" -type f -name '*.c' ! -name 'output.c' -exec awk '
/sixel_output_new[[:space:]]*\(/ {
    print FILENAME ":" FNR ":" $0
}
' {} + > "$violations"

if test -s "$violations"; then
    echo "not ok 1 - src emitter construction uses factory"
    sed 's/^/# direct constructor: /' "$violations"
    failed=1
else
    echo "ok 1 - src emitter construction uses factory"
fi

if awk '
/typedef struct sixel_output sixel_output_t;/ {
    seen_type = 1
}
/sixel_output_new[[:space:]]*\(/ {
    seen_new = 1
}
/sixel_output_unref[[:space:]]*\(/ {
    seen_unref = 1
}
/sixel_output_set_encode_policy[[:space:]]*\(/ {
    seen_policy = 1
}
END {
    exit (seen_type && seen_new && seen_unref && seen_policy) ? 0 : 1
}
' "$public_header"; then
    echo "ok 2 - legacy public output API stays exported"
else
    echo "not ok 2 - legacy public output API stays exported"
    echo "# missing sixel_output_t or sixel_output_* public API"
    failed=1
fi

if awk '
/^interface[ \t]+output[ \t]*[;{]/ {
    print "legacy interface output remains: " FNR
}
/^coclass[ \t]+output_component[ \t]*[{]/ {
    print "legacy coclass output_component remains: " FNR
}
/^interface[ \t]+sixel_emitter[ \t]*[;{]/ {
    seen_interface = 1
}
/^coclass[ \t]+sixel_emitter_component[ \t]*[{]/ {
    seen_coclass = 1
}
END {
    exit (seen_interface && seen_coclass) ? 0 : 1
}
' "$idl_file" > "$tmpdir/idl.txt"; then
    if test -s "$tmpdir/idl.txt"; then
        echo "not ok 3 - 6cells IDL uses sixel_emitter as canonical name"
        sed 's/^/# /' "$tmpdir/idl.txt"
        failed=1
    else
        echo "ok 3 - 6cells IDL uses sixel_emitter as canonical name"
    fi
else
    echo "not ok 3 - 6cells IDL uses sixel_emitter as canonical name"
    sed 's/^/# /' "$tmpdir/idl.txt"
    failed=1
fi

if awk '
/# define SIXEL_FACTORY_CLASSID_CREATE_[0-9]+ sixel_emitter_factory_new/ {
    macro = $3
    emitter_macro[macro] = 1
}
/^terminal\/sixel-emitter,[ \t]*SIXEL_FACTORY_CLASSID_CREATE_[0-9]+$/ {
    if ($2 in emitter_macro) {
        seen_canonical = 1
    }
}
/^terminal\/output,[ \t]*SIXEL_FACTORY_CLASSID_CREATE_[0-9]+$/ {
    if ($2 in emitter_macro) {
        seen_alias = 1
    }
}
END {
    exit (seen_canonical && seen_alias) ? 0 : 1
}
' "$classid_gperf"; then
    echo "ok 4 - emitter classid and legacy alias are registered"
else
    echo "not ok 4 - emitter classid and legacy alias are registered"
    echo "# terminal/sixel-emitter and terminal/output must resolve to emitter"
    failed=1
fi

find "$src_root/src" -type f \( -name '*.c' -o -name '*.h' \) \
    ! -name 'output.c' \
    ! -name 'output.h' \
    ! -name 'classid-factory.h' \
    ! -name 'classid-factory.gperf' \
    -exec awk '
/sixel_output_interface_t|sixel_output_vtbl_t|sixel_output_as_interface/ {
    print FILENAME ":" FNR ":" $0
}
/sixel_output_(writer_request|options|format)_t/ {
    print FILENAME ":" FNR ":" $0
}
' {} + > "$legacy_vtbl"

if test -s "$legacy_vtbl"; then
    echo "not ok 5 - src vtbl callers use sixel_emitter names"
    sed 's/^/# legacy output vtbl name: /' "$legacy_vtbl"
    failed=1
else
    echo "ok 5 - src vtbl callers use sixel_emitter names"
fi

exit "$failed"
