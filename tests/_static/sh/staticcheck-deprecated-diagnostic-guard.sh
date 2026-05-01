#!/bin/sh
# Emit TAP ensuring deprecated diagnostic pragmas use configured guards.

set -eu

src_root=${1:-}
configure_ac=$src_root/configure.ac
meson_build=$src_root/meson.build

echo "1..1"

if test -z "$src_root"; then
    echo "not ok 1 - deprecated diagnostic pragmas use configured guards"
    echo "# src_root argument is required"
    exit 1
fi

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-deprecated-guard-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

bad=$tmpdir/bad.txt
probe_bad=$tmpdir/probe-bad.txt
meson_probe_bad=$tmpdir/meson-probe-bad.txt

for scan_dir in "$src_root/src" "$src_root/include" "$src_root/tests"
do
    test -d "$scan_dir" || continue
    find "$scan_dir" -type f \( -name '*.c' -o -name '*.h' \) -print
done | LC_ALL=C sort | while IFS= read -r path
do
    awk '
    function starts_if(line) {
        return line ~ /^[[:space:]]*#[[:space:]]*(if|ifdef|ifndef)([[:space:]]|$)/
    }
    function starts_elif(line) {
        return line ~ /^[[:space:]]*#[[:space:]]*elif([[:space:]]|$)/
    }
    function starts_else(line) {
        return line ~ /^[[:space:]]*#[[:space:]]*else([[:space:]]|$)/
    }
    function starts_endif(line) {
        return line ~ /^[[:space:]]*#[[:space:]]*endif([[:space:]]|$)/
    }
    function has_deprecated_guard(line) {
        return line !~ /^[[:space:]]*#[[:space:]]*ifndef([[:space:]]|$)/ &&
            line ~ /HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS/
    }
    starts_if($0) {
        depth++
        guarded[depth] = guarded[depth - 1] || has_deprecated_guard($0)
        next
    }
    starts_elif($0) {
        guarded[depth] = guarded[depth - 1] || has_deprecated_guard($0)
        next
    }
    starts_else($0) {
        guarded[depth] = guarded[depth - 1]
        next
    }
    starts_endif($0) {
        delete guarded[depth]
        if (depth > 0) {
            depth--
        }
        next
    }
    /^[[:space:]]*#[[:space:]]*pragma[[:space:]]+(GCC|clang)[[:space:]]+diagnostic[[:space:]]+ignored[[:space:]]+"-Wdeprecated-declarations"/ {
        if (!guarded[depth]) {
            printf "%s:%d:unguarded -Wdeprecated-declarations pragma\n",
                FILENAME, NR
        }
    }
    ' "$path"
done > "$bad"

if test -f "$configure_ac"; then
    awk '
    /whether the compiler can silence -Wdeprecated-declarations/ {
        in_probe = 1
    }
    in_probe && /!defined\(__PCC__\)/ {
        saw_pcc_exclusion = 1
    }
    in_probe && /__attribute__\(\(deprecated\)\)/ {
        saw_deprecated_attribute = 1
    }
    in_probe && /return sixel_deprecated_probe\(\);/ {
        saw_deprecated_call = 1
    }
    END {
        if (!saw_pcc_exclusion) {
            print "configure.ac: deprecated diagnostic probe does not exclude __PCC__"
        }
        if (!saw_deprecated_attribute || !saw_deprecated_call) {
            print "configure.ac: deprecated diagnostic probe does not compile a deprecated call"
        }
    }
    ' "$configure_ac" > "$probe_bad"
else
    printf '%s\n' "configure.ac: missing file" > "$probe_bad"
fi

cat "$probe_bad" >> "$bad"

if test -f "$meson_build"; then
    awk '
    /Keep this in sync with configure\.ac.*-Wdeprecated-declarations probe/ {
        in_probe = 1
    }
    in_probe && /!defined\(__PCC__\)/ {
        saw_pcc_exclusion = 1
    }
    in_probe && /__attribute__\(\(deprecated\)\)/ {
        saw_deprecated_attribute = 1
    }
    in_probe && /return sixel_deprecated_probe\(\);/ {
        saw_deprecated_call = 1
    }
    in_probe && /args:[[:space:]]*\[[^]]*-Werror/ {
        saw_werror_probe = 1
    }
    in_probe && /conf.set.*HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS/ {
        saw_conf_set = 1
    }
    in_probe && /^endif/ && saw_conf_set {
        in_probe = 0
    }
    /^wflags = cc\.get_supported_arguments/ {
        in_wflags = 1
    }
    /^foreach f: wflags/ {
        in_wflags_loop = 1
    }
    in_wflags_loop && /HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS/ {
        saw_wflags_define = 1
    }
    /^endforeach/ && in_wflags_loop {
        in_wflags = 0
        in_wflags_loop = 0
    }
    END {
        if (!saw_pcc_exclusion) {
            print "meson.build: deprecated diagnostic probe does not exclude __PCC__"
        }
        if (!saw_deprecated_attribute || !saw_deprecated_call) {
            print "meson.build: deprecated diagnostic probe does not compile a deprecated call"
        }
        if (!saw_werror_probe) {
            print "meson.build: deprecated diagnostic probe does not use -Werror"
        }
        if (saw_wflags_define) {
            print "meson.build: warning flag support must not define deprecated diagnostic pragma support"
        }
    }
    ' "$meson_build" > "$meson_probe_bad"
else
    printf '%s\n' "meson.build: missing file" > "$meson_probe_bad"
fi

cat "$meson_probe_bad" >> "$bad"

if test -s "$bad"; then
    echo "not ok 1 - deprecated diagnostic pragmas use configured guards"
    sed 's/^/# /' "$bad"
    exit 1
fi

echo "ok 1 - deprecated diagnostic pragmas use configured guards"
exit 0
