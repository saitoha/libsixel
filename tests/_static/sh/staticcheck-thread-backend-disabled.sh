#!/bin/sh
# Serial builds must select stub synchronization before any native backend.
# Policy: docs/threading/encoder.md
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
src_root=$1
# Dollar signs belong to awk, not the invoking shell.
# shellcheck disable=SC2016
check='
/^[ \t]*#[ \t]*if/ { depth++; group[depth] = ++sequence }
/^[ \t]*#[ \t]*endif/ { depth-- }
/^#if !SIXEL_ENABLE_THREADS$/ {
    disabled = depth; owner = group[depth]; found++
}
disabled && /^#(elif|else)/ && depth == disabled { disabled = 0 }
/^# define SIXEL_USE_(PTHREADS|WIN32_THREADS) / {
    if (!owner || group[depth] != owner) bad = 1
    if (!first[$3]++) {
        if (!disabled || $4 != "0") bad = 1
        names++
    }
}
END { exit bad || found != 1 || names != 2 }
'
awk "$check" "$src_root/src/threading.c" || {
    echo 'not ok 1 - disabled threads must precede both native backends'
    exit 0
}
# Native Windows selection before the disabled branch must be rejected.
status=0
printf '%s\n' '#if defined(_WIN32)' '# define SIXEL_USE_PTHREADS 0' \
    '# define SIXEL_USE_WIN32_THREADS 1' '#endif' \
    '#if !SIXEL_ENABLE_THREADS' '# define SIXEL_USE_PTHREADS 0' \
    '# define SIXEL_USE_WIN32_THREADS 0' '#endif' | \
    awk "$check" || status=$?
test "$status" = 1 || {
    echo 'not ok 1 - native-first backend control escaped detection'
    exit 0
}
# A disabled branch that still enables Win32 synchronization is also wrong.
status=0
printf '%s\n' '#if !SIXEL_ENABLE_THREADS' '# define SIXEL_USE_PTHREADS 0' \
    '# define SIXEL_USE_WIN32_THREADS 1' '#endif' | \
    awk "$check" || status=$?
test "$status" = 1 || {
    echo 'not ok 1 - active serial backend control escaped detection'
    exit 0
}
echo 'ok 1 - disabled threads select stubs before native backends'
exit 0
