#!/bin/sh
# Emit TAP for aborttrace helper Wine skip ordering.
# Policy: docs/testing/guide.md

set -eu

src_root=$1
tests_root=$src_root/tests/cli/aborttrace

echo "1..1"

test -d "$tests_root" || {
    echo "not ok 1 - aborttrace helper tests skip Wine runtimes early"
    echo "# aborttrace test directory not found: $tests_root"
    exit 1
}

LC_ALL=C awk -v prefix="$src_root/" '
function relative_path(path) {
    if (index(path, prefix) == 1) {
        return substr(path, length(prefix) + 1)
    }
    return path
}
function finish_guard() {
    if (guard_kind == "") {
        return
    }
    if (!guard_has_skip || !guard_has_exit) {
        printf "# %s:%d: %s guard must emit 1..0 # SKIP and exit 0\n", \
            file, guard_line, guard_kind
        failed = 1
    } else if (guard_kind == "wine") {
        wine_line = guard_line
    } else {
        wine64_line = guard_line
    }
    guard_kind = ""
    guard_line = 0
    guard_has_skip = 0
    guard_has_exit = 0
}
function inspect_file() {
    finish_guard()
    if (!uses_helper) {
        return
    }
    helper_files += 1
    if (wine_line == 0) {
        printf "# %s: missing early wine skip\n", file
        failed = 1
    }
    if (wine64_line == 0) {
        printf "# %s: missing early wine64 skip\n", file
        failed = 1
    }
    if (normal_plan_line == 0) {
        printf "# %s: missing normal TAP plan\n", file
        failed = 1
    }
    if (wine_line != 0 && normal_plan_line != 0 && \
        wine_line >= normal_plan_line) {
        printf "# %s:%d: wine skip follows normal TAP plan at line %d\n", \
            file, wine_line, normal_plan_line
        failed = 1
    }
    if (wine64_line != 0 && normal_plan_line != 0 && \
        wine64_line >= normal_plan_line) {
        printf "# %s:%d: wine64 skip follows normal TAP plan at line %d\n", \
            file, wine64_line, normal_plan_line
        failed = 1
    }
    if (wine_line != 0 && wine_line >= helper_line) {
        printf "# %s:%d: wine skip follows aborttrace helper at line %d\n", \
            file, wine_line, helper_line
        failed = 1
    }
    if (wine64_line != 0 && wine64_line >= helper_line) {
        printf "# %s:%d: wine64 skip follows aborttrace helper at line %d\n", \
            file, wine64_line, helper_line
        failed = 1
    }
}
FNR == 1 {
    if (seen_file) {
        inspect_file()
    }
    seen_file = 1
    file = relative_path(FILENAME)
    uses_helper = 0
    helper_line = 0
    normal_plan_line = 0
    wine_line = 0
    wine64_line = 0
    guard_kind = ""
    guard_line = 0
    guard_has_skip = 0
    guard_has_exit = 0
}
{
    line = $0
    if (line ~ /aborttrace\/0001_img2sixel_aborttrace/) {
        uses_helper = 1
        if (helper_line == 0) {
            helper_line = FNR
        }
    }
    if (normal_plan_line == 0 && line ~ /1\.\.1/) {
        normal_plan_line = FNR
    }
    if (guard_kind == "" && \
        line ~ /^[[:space:]]*test[[:space:]]+/ && \
        line ~ /\$\{SIXEL_RUNTIME-\}/ && \
        line ~ /=[[:space:]]*"wine"[[:space:]]*&&[[:space:]]*\{/) {
        guard_kind = "wine"
        guard_line = FNR
        next
    }
    if (guard_kind == "" && \
        line ~ /^[[:space:]]*test[[:space:]]+/ && \
        line ~ /\$\{SIXEL_RUNTIME-\}/ && \
        line ~ /=[[:space:]]*"wine64"[[:space:]]*&&[[:space:]]*\{/) {
        guard_kind = "wine64"
        guard_line = FNR
        next
    }
    if (guard_kind != "") {
        if (line ~ /1\.\.0[[:space:]]*#[[:space:]]*SKIP/) {
            guard_has_skip = 1
        }
        if (line ~ /^[[:space:]]*exit[[:space:]]+0([[:space:]]|$)/) {
            guard_has_exit = 1
        }
        if (line ~ /^[[:space:]]*}/) {
            finish_guard()
        }
    }
}
END {
    if (seen_file) {
        inspect_file()
    }
    if (helper_files == 0) {
        print "# no aborttrace helper TAP tests found"
        failed = 1
    }
    exit failed ? 1 : 0
}
' "$tests_root"/*.t || {
    echo "not ok 1 - aborttrace helper tests skip Wine runtimes early"
    exit 1
}

echo "ok 1 - aborttrace helper tests skip Wine runtimes early"
exit 0
