#!/bin/sh
# Keep a multiline object response file intact through clang-cl to link.exe.
# Policy: docs/build/test-harness.md
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
src_root=$1
# The flag must be selected specifically for clang-cl and tested by linking
# the referenced object, rather than inferred from option acceptance.
awk '
/AS_IF\(\[test "x\$using_clangcl" = xyes\], \[/ { clangcl = 1; next }
clangcl && /sixel_response_flag=-clang:-Wl,@/ { flag = 1 }
clangcl && /\]\)/ { clangcl = 0 }
/LIBS="\$\{sixel_response_flag\}conftest-response.objects \$LIBS"/ { input = 1 }
/return sixel_response_probe\(\)/ { reference = 1 }
/sixel_cv_cc_object_response_flag=\$sixel_response_flag/ { probed = 1 }
/SIXEL_TEST_RUNNER_RESPONSE_FLAG=\$sixel_cv_cc_object_response_flag/ { result = 1 }
/^SIXEL_TEST_RUNNER_RESPONSE_FLAG = @SIXEL_TEST_RUNNER_RESPONSE_FLAG@$/ { subst = 1 }
END { exit !(flag && input && reference && probed && result && subst) }
' "$src_root/configure.ac" "$src_root/tests/Makefile.am" || {
    echo 'not ok 1 - clang-cl linker forwarding must have a link probe'
    exit 0
}
fixture=$(mktemp -d "${TMPDIR:-/tmp}/sixel-link-response-XXXXXX")
trap 'rm -rf "$fixture"' EXIT HUP INT TERM
make_bin=${SIXEL_STATICCHECK_MAKE:-make}
command -v gmake >/dev/null 2>&1 && make_bin=${SIXEL_STATICCHECK_MAKE:-gmake}
# Repeated long paths exceed a native response line limit without creating
# thousands of objects. The mock compiler observes the exact driver input.
awk -v root="$src_root" -v work="$fixture" 'BEGIN {
    name = sprintf("%0160d.o", 0)
    printf "" > (work "/" name)
    printf "test_runner_OBJECTS ="
    for (i = 0; i < 1024; i++) printf " %s", name
    print "\nSIXEL_TEST_RUNNER_USE_RESPONSE_FILE = yes"
    print "SIXEL_TEST_RUNNER_RESPONSE_FLAG = -clang:-Wl,@"
    print "test_runner_LINK = $(SHELL) verify.sh"
    print "include " root "/build-aux/test-runner-link.am"
}' > "$fixture/Makefile"
cat > "$fixture/verify.sh" <<'DRIVER'
#!/bin/sh
set -eu
test "$#" = 1
test "$1" = '-clang:-Wl,@test_runner.objects'
awk 'length($0) > 1024 { bad = 1 } { bytes += length($0) + 1 }
    END { exit bad || NR != 1024 || bytes <= 131072 }' test_runner.objects
DRIVER
unset MAKEFLAGS MFLAGS MAKELEVEL
"$make_bin" -s --no-print-directory -C "$fixture" test_runner >&2 || {
    echo 'not ok 1 - link recipe must preserve the linker response file'
    exit 0
}
# A driver-expanded @file must be rejected by the same observation.
"$make_bin" -s --no-print-directory -C "$fixture" \
    SIXEL_TEST_RUNNER_RESPONSE_FLAG=@ test_runner >&2 && {
    echo 'not ok 1 - compiler response expansion escaped detection'
    exit 0
}
echo 'ok 1 - clang-cl forwards the bounded object file to the linker'
exit 0
