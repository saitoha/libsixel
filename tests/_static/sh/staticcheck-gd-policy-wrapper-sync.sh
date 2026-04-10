#!/bin/sh
# Emit TAP for gd policy mode definitions and TAP wrappers staying synchronized.

set -eu

echo "1..1"

src_root=$1
can_try_source=$src_root/tests/loader/0057_loader_gd_can_try_policy.c
status_source=$src_root/tests/loader/0058_loader_gd_status_policy.c
gd_tests_dir=$src_root/tests/loader/gd

if test ! -f "$can_try_source" || test ! -f "$status_source" ||
        test ! -d "$gd_tests_dir"; then
    echo "ok 1 # SKIP missing gd policy sources or gd test directory"
    exit 0
fi

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-staticcheck-gd-policy-sync-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

can_try_modes=$tmpdir/can_try_modes.txt
status_modes=$tmpdir/status_modes.txt
can_try_wrappers=$tmpdir/can_try_wrappers.txt
status_wrappers=$tmpdir/status_wrappers.txt
can_try_wrapper_map=$tmpdir/can_try_wrapper_map.tsv
status_wrapper_map=$tmpdir/status_wrapper_map.tsv
missing=$tmpdir/missing.txt
status=0

extract_modes_from_source() {
    source_path=$1
    output_path=$2

    awk '
    {
        if (match($0, /strcmp\(mode,[[:space:]]*"[^"]+"/)) {
            token = substr($0, RSTART, RLENGTH)
            sub(/^.*strcmp\(mode,[[:space:]]*"/, "", token)
            sub(/"$/, "", token)
            if (token != "" && token != "all") {
                print token
            }
        }
    }
    ' "$source_path" | LC_ALL=C sort -u > "$output_path"
}

extract_wrapper_modes() {
    target_name=$1
    output_path=$2
    map_path=$3

    : > "$map_path"
    for test_path in "$gd_tests_dir"/*.t; do
        test -f "$test_path" || continue
        awk -v target="$target_name" -v path="$test_path" '
        {
            line = $0
            while (match(line, /"[^"]*"/)) {
                token = substr(line, RSTART + 1, RLENGTH - 2)
                if (want_mode == 1) {
                    printf "%s\t%s\n", token, path >> map
                    want_mode = 0
                }
                if (token == target) {
                    want_mode = 1
                }
                line = substr(line, RSTART + RLENGTH)
            }
        }
        ' map="$map_path" "$test_path"
    done

    LC_ALL=C sort -u "$map_path" | awk -F '\t' '{ print $1 }' > "$output_path"
}

extract_modes_from_source "$can_try_source" "$can_try_modes"
extract_modes_from_source "$status_source" "$status_modes"
extract_wrapper_modes "loader/0057_loader_gd_can_try_policy" \
    "$can_try_wrappers" "$can_try_wrapper_map"
extract_wrapper_modes "loader/0058_loader_gd_status_policy" \
    "$status_wrappers" "$status_wrapper_map"

for mode_name in $(comm -23 "$can_try_modes" "$can_try_wrappers"); do
    echo "# gd can_try mode lacks TAP wrapper: $mode_name" >> "$missing"
    status=1
done
for mode_name in $(comm -13 "$can_try_modes" "$can_try_wrappers"); do
    echo "# gd can_try wrapper references unknown mode: $mode_name" >> "$missing"
    status=1
done
for mode_name in $(comm -23 "$status_modes" "$status_wrappers"); do
    echo "# gd status mode lacks TAP wrapper: $mode_name" >> "$missing"
    status=1
done
for mode_name in $(comm -13 "$status_modes" "$status_wrappers"); do
    echo "# gd status wrapper references unknown mode: $mode_name" >> "$missing"
    status=1
done

awk -F '\t' '
{
    count[$1] += 1
}
END {
    for (mode in count) {
        if (count[mode] > 1) {
            printf "# gd can_try mode has multiple wrappers: %s (%d)\n",
                   mode,
                   count[mode]
        }
    }
}
' "$can_try_wrapper_map" >> "$missing"

awk -F '\t' '
{
    count[$1] += 1
}
END {
    for (mode in count) {
        if (count[mode] > 1) {
            printf "# gd status mode has multiple wrappers: %s (%d)\n",
                   mode,
                   count[mode]
        }
    }
}
' "$status_wrapper_map" >> "$missing"

if test -s "$missing"; then
    status=1
fi

if test "$status" -eq 0; then
    echo "ok 1 - gd policy modes and TAP wrappers stay synchronized"
    exit 0
fi

echo "not ok 1 - gd policy modes and TAP wrappers stay synchronized"
cat "$missing"
exit 1
