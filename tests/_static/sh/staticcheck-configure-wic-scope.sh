#!/bin/sh
# WIC probes must restore flags only on the hosts that saved them.
# Policy: docs/build/test-harness.md
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
awk '
function valid(text,    windows, other, boundary) {
    boundary = index(text, ";;")
    windows = substr(text, 1, boundary - 1)
    other = substr(text, boundary + 2)
    return index(windows, "sixel_wic_save_CFLAGS=\"$CFLAGS\"") &&
        index(windows, "sixel_wic_save_LIBS=\"$LIBS\"") &&
        index(windows, "CFLAGS=$sixel_wic_save_CFLAGS") &&
        index(windows, "LIBS=$sixel_wic_save_LIBS") &&
        other !~ /(CFLAGS|LIBS)=/ && text !~ /(CFLAGS|LIBS)_BACKUP/
}
/^if test x\$with_wic != xno; then/ { active = 1 }
active { text = text $0 "\n" }
/^AC_SUBST\(WIC_CFLAGS\)/ { active = 0 }
END {
    lost = text
    sub(/LIBS=\$sixel_wic_save_LIBS/, "", lost)
    unsaved = text
    sub(/sixel_wic_save_CFLAGS="\$CFLAGS"/, "", unsaved)
    exit !valid(text) || valid(lost) || valid(unsaved) ||
        valid(text "\nLIBS=$sixel_wic_save_LIBS\n")
}
' "$1/configure.ac" || {
    echo 'not ok' '1 - WIC flag restoration stays inside the Windows branch'
    exit 0
}
echo 'ok' '1 - WIC flag restoration stays inside the Windows branch'
exit 0
