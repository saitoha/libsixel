#!/bin/sh
# Emit TAP for timeline logging component boundary checks.

set -eu

src_root=${1:-}

echo "1..1"

if test -z "$src_root"; then
    echo "not ok 1 - timeline logging stays componentized"
    echo "# src_root argument is required"
    exit 1
fi

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-timeline-boundary-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

report=$tmpdir/report.txt

: >"$report"

{
test ! -e "$src_root/src/logger.c" || {
    echo "src/logger.c must not be restored"
}
test ! -e "$src_root/src/logger.h" || {
    echo "src/logger.h must not be restored"
}

find "$src_root/src" "$src_root/include" "$src_root/tests" \
    -type f \( -name '*.[ch]' -o -name '*.inc.c' \) -print |
awk -v src_root="$src_root" '
function rel(path) {
    sub("^" src_root "/", "", path)
    return path
}
FILENAME != last_file {
    last_file = FILENAME
    path = rel(FILENAME)
}
/# *include[ \t]+"logger\.h"/ {
    print path ":" FNR ": old logger.h include is forbidden"
}
	/sixel_logger_t/ {
	    print path ":" FNR ": old sixel_logger_t type is forbidden"
	}
	/sixel_logger_/ {
	    print path ":" FNR ": old sixel_logger_ symbol is forbidden"
	}
/sixel_timeline_logger_t[ \t]+[A-Za-z_][A-Za-z0-9_]*[ \t]*;/ {
    print path ":" FNR ": timeline logger must be held by pointer"
}
/sixel_timeline_logger_is_enabled/ {
    print path ":" FNR ": disabled logging must be represented by NULL logger"
}
/sixel_timeline_logger_flush/ {
    print path ":" FNR ": timeline logger must not own flush responsibility"
}
/timeline_logger.*enabled|timeline_logger.*flush/ {
    print path ":" FNR ": timeline_logger IDL must not expose enabled or flush"
}
/timeline_writer.*enabled/ {
    print path ":" FNR ": timeline_writer IDL must not expose enabled"
}
/logger->[ \t]*(active|file|delegate)/ {
    print path ":" FNR ": timeline logger direct field access is forbidden"
}
FILENAME ~ /\/timeline-logger\.c$/ && /fflush[ \t]*\(/ {
    print path ":" FNR ": timeline logger must not flush per event"
}
FILENAME ~ /\/timeline-writer\.c$/ &&
    /static[ \t]+sixel_timeline_writer_storage_t[ \t]+g_sixel_timeline_writer[ \t]*=/ {
    print path ":" FNR ": timeline writer singleton must use zero init"
}
FILENAME ~ /\/tests\/processing\/timeline\/.*\.c$/ &&
    /# *include[ \t]+"src\/sleep\.h"/ {
    print path ":" FNR ": timeline tests must not include internal sleep helper"
}
FILENAME ~ /\/tests\/processing\/timeline\/.*\.c$/ &&
    /sixel_sleep[ \t]*\(/ {
    print path ":" FNR ": timeline tests must not link internal sleep helper"
}
'

find "$src_root/amalgamation" "$src_root/wic" "$src_root/src" \
    -type f \( -name 'Makefile.am' -o -name 'Makefile.in' \
        -o -name 'meson.build' \) -print |
awk -v src_root="$src_root" '
function rel(path) {
    sub("^" src_root "/", "", path)
    return path
}
FILENAME != last_file {
    last_file = FILENAME
    path = rel(FILENAME)
}
/(^|[^A-Za-z0-9_-])logger\.(c|h)([^A-Za-z0-9_-]|$)/ {
    print path ":" FNR ": old logger build input is forbidden"
}
'

awk '
/sixel_timeline_writer_init_storage[ \t]*\(/ {
    count += 1
}
END {
    if (count < 2) {
        print "src/timeline-writer.c: timeline writer storage init must be called"
    }
}
' "$src_root/src/timeline-writer.c"

awk '
/^sixel_encode_dither\(/ {
    in_func = 1
    saw_serial_init = 0
}
in_func && /serial_logger[ \t]*=[ \t]*NULL[ \t]*;/ {
    saw_serial_init = 1
}
in_func && /palette_source_colorspace[ \t]*=[ \t]*SIXEL_COLORSPACE_GAMMA[ \t]*;/ {
    if (!saw_serial_init) {
        print "src/tosixel.c:" FNR \
            ": serial_logger must be initialized before early cleanup paths"
    }
    in_func = 0
}
' "$src_root/src/tosixel.c"

awk '
/^timeline_parallel_encode_decode\(void\)/ {
    in_func = 1
    saw_log_path_init = 0
}
in_func && /log_path[ \t]*=[ \t]*NULL[ \t]*;/ {
    saw_log_path_init = 1
}
in_func && /timeline_build_source_path\(/ {
    if (!saw_log_path_init) {
        print "tests/processing/timeline/" \
            "0002_timeline_parallel_encode_decode.c:" FNR \
            ": log_path must be initialized before early cleanup paths"
    }
    in_func = 0
}
' "$src_root/tests/processing/timeline/0002_timeline_parallel_encode_decode.c"
} >>"$report"

if test -s "$report"; then
    echo "not ok 1 - timeline logging stays componentized"
    sed 's/^/# /' "$report"
    exit 1
fi

echo "ok 1 - timeline logging stays componentized"
exit 0
