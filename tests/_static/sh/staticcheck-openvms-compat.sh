#!/bin/sh
# Verify the source-level OpenVMS compatibility boundaries.
# Policy: docs/misc/platforms/openvms.md
# Coverage: OV-01 OV-02 OV-03 OV-04 OV-05

set -eu

src_root=$1
failed=0

fail()
{
    echo "# $*" >&2
    failed=1
}

require_fixed()
{
    pattern=$1
    path=$2

    grep -F -- "$pattern" "$src_root/$path" >/dev/null 2>&1 ||
        fail "$path is missing: $pattern"
}

echo "1..1"

openvms_detect_line=$(awk '/^sixel_openvms_mode=no$/ { print NR; exit }' \
    "$src_root/configure.ac")
canonical_host_line=$(awk '/^AC_CANONICAL_HOST$/ { print NR; exit }' \
    "$src_root/configure.ac")
test -n "$openvms_detect_line" && test -n "$canonical_host_line" &&
    test "$openvms_detect_line" -lt "$canonical_host_line" ||
    fail "configure must detect OpenVMS before AC_CANONICAL_HOST"

require_fixed \
    "ac_executable_extensions=\".exe \$ac_executable_extensions\"" configure.ac
require_fixed 'ac_check_headers_parallel_max=1' configure.ac
require_fixed 'ac_check_funcs_parallel_max=1' configure.ac
require_fixed 'CPU_COUNT=1' configure.ac
require_fixed 'ac_config_status_jobs=""' configure.ac
require_fixed 'enable_dependency_tracking=no' configure.ac
require_fixed 'openvms/gnv-ar.sh' configure.ac
require_fixed 'openvms/gnv-cc.sh' configure.ac
require_fixed 'openvms/gnv-rm.sh' configure.ac
require_fixed 'if ($[0] !~ /\\$/)' configure.ac

require_fixed 'LS_OPENVMS_BUILD_MAKEFLAGS = -j1 V=1' Makefile.am
require_fixed 'OPENVMS_TEST_JOBS ?= 1' tests/Makefile.am
require_fixed "for obj in \$(libsixel_la_OBJECTS); do" src/Makefile.am

openvms_link_count=$(grep -hF \
    "gnv-link-program.sh -o \$@" \
    "$src_root/converters/Makefile.am" \
    "$src_root/assessment/Makefile.am" \
    "$src_root/tools/Makefile.am" \
    "$src_root/tests/Makefile.am" | awk 'END { print NR }')
test "$openvms_link_count" -eq 6 ||
    fail "expected 6 native OpenVMS program-link overrides, found $openvms_link_count"

/bin/sh "$src_root/openvms/gnv-ar.sh" /bin/sh -c \
    'printf "%s\n" "%LIBRAR-W-COMCOD, warning"; exit 3' >/dev/null ||
    fail "gnv-ar.sh did not accept a warning-only condition"
if /bin/sh "$src_root/openvms/gnv-ar.sh" /bin/sh -c \
    'printf "%s\n" "%LIBRAR-E-FAILED, error"; exit 2' >/dev/null 2>&1; then
    fail "gnv-ar.sh accepted an error condition"
fi

/bin/sh "$src_root/openvms/gnv-cc.sh" /bin/sh -c \
    'printf "%s\n" "%CC-W-WARN, warning"; exit 3' >/dev/null ||
    fail "gnv-cc.sh did not accept a warning-only condition"
if /bin/sh "$src_root/openvms/gnv-cc.sh" /bin/sh -c \
    'printf "%s\n" "error: rejected"; exit 2' >/dev/null 2>&1; then
    fail "gnv-cc.sh accepted an error condition"
fi

missing_path="$src_root/tests/_static/openvms-rm-missing"
false_bin=$(command -v false)
test ! -e "$missing_path" || fail "static-check sentinel unexpectedly exists"
GNV_RM="$false_bin" /bin/sh "$src_root/openvms/gnv-rm.sh" -f \
    "$missing_path" ||
    fail "gnv-rm.sh did not ignore a missing forced-removal operand"

require_fixed '%ILINK-W-NUDFSYMS' openvms/gnv-link-program.sh
require_fixed '%ILINK-W-USEUNDEF' openvms/gnv-link-program.sh
require_fixed "if test -f \"\$out\"; then" openvms/gnv-link-program.sh
require_fixed "die \"unsupported link argument: \$arg\"" \
    openvms/gnv-link-program.sh
if TMPDIR="${TMPDIR:-/tmp}" /bin/sh \
        "$src_root/openvms/gnv-link-program.sh" \
        -o libsixel-openvms-staticcheck.exe unsupported-link-input \
        >/dev/null 2>&1; then
    fail "gnv-link-program.sh accepted an unsupported argument"
fi
if TMPDIR="${TMPDIR:-/tmp}" /bin/sh \
        "$src_root/openvms/gnv-link-program.sh" -o \
        libsixel-openvms-staticcheck.exe >/dev/null 2>&1; then
    fail "gnv-link-program.sh accepted a link with no inputs"
fi

require_fixed '0x10000000' converters/img2sixel.c
require_fixed 'IMG2SIXEL_OPENVMS_INHIBIT_MSG | 2' converters/img2sixel.c
require_fixed 'IMG2SIXEL_OPENVMS_INHIBIT_MSG | 4' converters/img2sixel.c
require_fixed '0x10000000' converters/sixel2png.c
require_fixed 'SIXEL2PNG_OPENVMS_INHIBIT_MSG | 2' converters/sixel2png.c
require_fixed 'SIXEL2PNG_OPENVMS_INHIBIT_MSG | 4' converters/sixel2png.c
require_fixed 'SIXEL_TEST_MAX_MAPPED_ERROR_STATUS=4' \
    build-aux/lso-tap-driver.sh.in

test "$failed" -eq 0 || {
    echo "not ok 1 - OpenVMS compatibility boundaries are preserved"
    exit 1
}

echo "ok 1 - OpenVMS compatibility boundaries are preserved"
exit 0
