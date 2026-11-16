#!/bin/sh
# Ensure instrumented PTY fixtures launch the target without a gcov dump.
set -eux
echo '1..1'
set -v

src_root=$1
build_root=$2
{ test -f "$build_root/config.h" && command -v clang >/dev/null 2>&1; } || {
    echo 'ok 1 # SKIP configured headers or Clang unavailable'
    exit 0
}
awk '
$1 == "#define" && $3 == 1 { enabled[$2] = 1 }
END { exit !(enabled["HAVE_CLI_COLOR_PTY"] && enabled["HAVE_CLI_COLOR_TTY"]) }
' "$build_root/config.h" || {
    echo 'ok 1 # SKIP scripted POSIX terminal I/O is unavailable'
    exit 0
}
# Profile arcs exercise gcov lowering without emitting .gcno files.
set -- -O2 -std=c99 -DHAVE_CONFIG_H -fprofile-arcs -S -emit-llvm -o - \
    -I"$build_root" -I"$src_root"
{
    clang "$@" "$src_root/tests/_support/cli/0039_cli_color_auto.c"
    clang "$@" "$src_root/tests/cli/img2sixel/0033_stdin_interrupt.c"
} | awk '
/^@__llvm_gcov_ctr =/ { profiled++ }
/^define .*@test_cli_0039_color_auto\(/ { auto++ }
/^define .*@test_cli_stdin_interrupt\(/ { interrupt++ }
/call .*@(llvm_writeout_files|__gcov_exec[a-z]*)\(/ {
    print "# coverage write before PTY exec"
    blocked = 1
}
END { exit blocked || profiled != 2 || auto != 1 || interrupt != 1 }
' || {
    echo 'not ok 1 - PTY targets start without a coverage write'
    exit 0
}
echo 'ok 1 - PTY targets start without a coverage write'
exit 0
