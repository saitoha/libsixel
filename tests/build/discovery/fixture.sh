#!/bin/sh
# Load the production Automake branch into a small source-discovery fixture.
src_root=$1
work=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-discovery-XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
source_root=$work/source\ tree
mkdir -p "$source_root"
make_bin=${SIXEL_STATICCHECK_MAKE:-make}
command -v gmake >/dev/null 2>&1 && make_bin=${SIXEL_STATICCHECK_MAKE:-gmake}
{
    printf 'top_srcdir = %s\nsrcdir = %s\nbuilddir = .\nSHELL = /bin/sh\n' "$src_root" "$source_root"
    awk -v openvms="${openvms:-0}" '
        /^# Keep OpenVMS inventories/ { block = 1; enabled = 1 }
        /^XFAIL_TESTS =/ { block = 0 }
        block {
            if ($0 == "if OPENVMS_GNV") enabled = openvms
            else if ($0 == "if !OPENVMS_GNV") enabled = !openvms
            else if ($0 == "endif") enabled = 1
            else if (enabled) print
        }
        openvms && /^\.PHONY: sixel-check-test-list$/ { rule = 1 }
        /^sixel-check-test-logs:/ { rule = 0 }
        rule { print }
        END {
            if (openvms) print "inventory: sixel-check-test-list\n\t@cat \"$(OPENVMS_CHECK_TESTS_LIST)\""
            else print "inventory:\n\t@printf '\''%s\\n'\'' $(TESTS)"
        }
    ' "$src_root/tests/Makefile.am"
} > "$work/Makefile"
add() { mkdir -p "$source_root/${1%/*}"; : > "$source_root/$1"; }
run() (
    unset MAKEFLAGS MFLAGS MAKELEVEL TESTS SIXEL_INSTALLCHECK_GENERIC
    "$make_bin" -s --no-print-directory -C "$work" inventory "$@"
)
expect() {
    expected=$1
    shift
    actual=$(run "$@") || return 1
    test "$actual" = "$expected" || {
        printf 'expected <%s>, got <%s>\n' "$expected" "$actual" >&2
        return 1
    }
}
fail_scan() {
    mkdir -p "$work/bin"
    printf '%s\n' '#!/bin/sh' "printf './sample/existing/0001_existing.t\\n'" 'exit 1' > "$work/bin/sort"
    chmod +x "$work/bin/sort"
    PATH=$work/bin:$PATH
    export PATH
}
reject_scan() { result=0; run >/dev/null || result=$?; test "$result" -ne 0; }
