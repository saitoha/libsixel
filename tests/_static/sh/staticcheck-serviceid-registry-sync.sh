#!/bin/sh
# Emit TAP for service-id annotation and gperf sync checks.

set -eu

src_root=$1
generator=$src_root/tools/gen_serviceid_gperf.awk
gperf_file=$src_root/src/classid-service.gperf
idl_file=$src_root/include/6cells.idl

echo "1..2"

if test ! -f "$generator"; then
    echo "not ok 1 - components serviceid registry stays in sync"
    echo "# missing generator: $generator"
    echo "not ok 2 - service ids resolve to registry services"
    echo "# missing generator: $generator"
    exit 1
fi

if test ! -f "$gperf_file"; then
    echo "not ok 1 - components serviceid registry stays in sync"
    echo "# missing gperf file: $gperf_file"
    echo "not ok 2 - service ids resolve to registry services"
    echo "# missing gperf file: $gperf_file"
    exit 1
fi

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-serviceid-sync-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

expected=$tmpdir/classid-service.expected.gperf
failed=0

if ! awk -f "$generator" \
    "$src_root"/src/factory.h \
    "$src_root"/src/timeline-writer.h \
    >"$expected"; then
    echo "not ok 1 - components serviceid registry stays in sync"
    echo "# failed to regenerate classid-service.gperf"
    failed=1
elif cmp -s "$gperf_file" "$expected"; then
    echo "ok 1 - components serviceid registry stays in sync"
else
    echo "not ok 1 - components serviceid registry stays in sync"
    if command -v diff >/dev/null 2>&1; then
        diff -u "$gperf_file" "$expected" | sed 's/^/# /'
    else
        echo "# diff not found"
    fi
    failed=1
fi

if test ! -f "$idl_file"; then
    echo "not ok 2 - service ids resolve to registry services"
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
    if (line ~ /^\[serviceid\("[A-Za-z0-9_.\/-]+"\)\]$/) {
        pending_serviceid = line
        sub(/^\[serviceid\("/, "", pending_serviceid)
        sub(/"\)\]$/, "", pending_serviceid)
        next
    }
    if (line ~ /^interface[ \t]+[A-Za-z_][A-Za-z0-9_]*[ \t]*\{$/) {
        iface = line
        sub(/^interface[ \t]+/, "", iface)
        sub(/[ \t]*\{$/, "", iface)
        if (pending_serviceid != "") {
            service_by_id[pending_serviceid] = iface
            services[++service_count] = pending_serviceid
        }
        pending_serviceid = ""
        next
    }
    if (line != "" &&
        line !~ /^\/\// &&
        line !~ /^\/\*/ &&
        line !~ /^\*/ &&
        line !~ /^\[[^]]+\]$/) {
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
        getters[macro] = 1
        next
    }
    if (line ~ /^[A-Za-z0-9_.\/-]+,[ \t]*[A-Za-z0-9_]+$/) {
        serviceid = line
        sub(/,[ \t]*[A-Za-z0-9_]+$/, "", serviceid)
        macro = line
        sub(/^[A-Za-z0-9_.\/-]+,[ \t]*/, "", macro)
        registry_macro[serviceid] = macro
    }
}
END {
    if (service_count == 0) {
        print "missing serviceid definitions"
    }
    for (i = 1; i <= service_count; ++i) {
        serviceid = services[i]
        if (!(serviceid in registry_macro)) {
            print "missing service registry entry: " serviceid \
                " (" service_by_id[serviceid] ")"
            continue
        }
        macro = registry_macro[serviceid]
        if (!(macro in getters)) {
            print "missing nonzero service getter: " serviceid \
                " -> " macro
        }
    }
}
' "$idl_file" "$gperf_file" >"$tmpdir/serviceid-registry.txt"; then
    if test -s "$tmpdir/serviceid-registry.txt"; then
        echo "not ok 2 - service ids resolve to registry services"
        sed 's/^/# /' "$tmpdir/serviceid-registry.txt"
        failed=1
    else
        echo "ok 2 - service ids resolve to registry services"
    fi
else
    echo "not ok 2 - service ids resolve to registry services"
    sed 's/^/# /' "$tmpdir/serviceid-registry.txt"
    failed=1
fi

exit "$failed"
