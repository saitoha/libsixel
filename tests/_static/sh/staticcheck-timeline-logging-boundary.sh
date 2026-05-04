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
FILENAME ~ /\/tests\/processing\/timeline\/.*\.c$/ &&
    /char[ \t]+const[ \t]+\*[ \t]*log_path[ \t]*;/ {
    print path ":" FNR ": timeline tests must copy SIXEL_LOG_PATH"
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
    saw_log_path_zero = 0
    saw_log_path_copy = 0
}
in_func && /memset[ \t]*\([ \t]*log_path[ \t]*,/ {
    saw_log_path_zero = 1
}
in_func && /sixel_compat_strcpy[ \t]*\([ \t]*log_path[ \t]*,/ {
    saw_log_path_copy = 1
}
in_func && /timeline_build_source_path\(/ {
    if (!saw_log_path_zero) {
        print "tests/processing/timeline/" \
            "0002_timeline_parallel_encode_decode.c:" FNR \
            ": log_path must be zeroed before early cleanup paths"
    }
}
in_func && /sixel_mutex_init[ \t]*\(/ {
    if (!saw_log_path_copy) {
        print "tests/processing/timeline/" \
            "0002_timeline_parallel_encode_decode.c:" FNR \
            ": SIXEL_LOG_PATH must be copied before writer use"
    }
    in_func = 0
}
' "$src_root/tests/processing/timeline/0002_timeline_parallel_encode_decode.c"

awk '
/^timeline_clock_origin_is_shared\(void\)/ {
    in_func = 1
    saw_log_path_zero = 0
    saw_log_path_copy = 0
}
in_func && /memset[ \t]*\([ \t]*log_path[ \t]*,/ {
    saw_log_path_zero = 1
}
in_func && /sixel_compat_strcpy[ \t]*\([ \t]*log_path[ \t]*,/ {
    saw_log_path_copy = 1
}
in_func && /sixel_allocator_new[ \t]*\(/ {
    if (!saw_log_path_zero) {
        print "tests/processing/timeline/" \
            "0003_timeline_clock_origin.c:" FNR \
            ": log_path must be zeroed before early cleanup paths"
    }
    if (!saw_log_path_copy) {
        print "tests/processing/timeline/" \
            "0003_timeline_clock_origin.c:" FNR \
            ": SIXEL_LOG_PATH must be copied before writer use"
    }
    in_func = 0
}
' "$src_root/tests/processing/timeline/0003_timeline_clock_origin.c"

awk '
/timeline\/0002_timeline_parallel_encode_decode_verify/ {
    saw_verify = 1
    if (previous2 !~ /SIXEL_LOG_PATH="\$\{log_path\}"/) {
        print "tests/processing/timeline/" \
            "0002_timeline_parallel_encode_decode.t:" FNR \
            ": verifier must receive SIXEL_LOG_PATH as child env"
    }
    if (previous ~ /--env[ \t]+SIXEL_LOG_PATH=/ || previous2 ~ /--env[ \t]+SIXEL_LOG_PATH=/) {
        print "tests/processing/timeline/" \
            "0002_timeline_parallel_encode_decode.t:" FNR \
            ": verifier must not rely on test_runner --env"
    }
    watching_verify = 1
    next
}
watching_verify && /\$\{log_path\}/ {
    print "tests/processing/timeline/" \
        "0002_timeline_parallel_encode_decode.t:" FNR \
        ": verifier must not receive log path as argv"
    watching_verify = 0
}
watching_verify && /\|\|[ \t]*\{/ {
    watching_verify = 0
}
{
    previous2 = previous
    previous = $0
}
END {
    if (!saw_verify) {
        print "tests/processing/timeline/" \
            "0002_timeline_parallel_encode_decode.t:" \
            ": timeline JSONL must be verified after producer exit"
    }
}
' "$src_root/tests/processing/timeline/0002_timeline_parallel_encode_decode.t"

awk '
/timeline\/0003_timeline_clock_origin_verify/ {
    saw_verify = 1
    if (previous2 !~ /SIXEL_LOG_PATH="\$\{log_path\}"/) {
        print "tests/processing/timeline/" \
            "0003_timeline_clock_origin.t:" FNR \
            ": verifier must receive SIXEL_LOG_PATH as child env"
    }
    if (previous ~ /--env[ \t]+SIXEL_LOG_PATH=/ || previous2 ~ /--env[ \t]+SIXEL_LOG_PATH=/) {
        print "tests/processing/timeline/" \
            "0003_timeline_clock_origin.t:" FNR \
            ": verifier must not rely on test_runner --env"
    }
    watching_verify = 1
    next
}
watching_verify && /\$\{log_path\}/ {
    print "tests/processing/timeline/" \
        "0003_timeline_clock_origin.t:" FNR \
        ": verifier must not receive log path as argv"
    watching_verify = 0
}
watching_verify && /\|\|[ \t]*\{/ {
    watching_verify = 0
}
{
    previous2 = previous
    previous = $0
}
END {
    if (!saw_verify) {
        print "tests/processing/timeline/" \
            "0003_timeline_clock_origin.t:" \
            ": timeline JSONL must be verified after producer exit"
    }
}
' "$src_root/tests/processing/timeline/0003_timeline_clock_origin.t"
} >>"$report"

if test -s "$report"; then
    echo "not ok 1 - timeline logging stays componentized"
    sed 's/^/# /' "$report"
    exit 1
fi

echo "ok 1 - timeline logging stays componentized"
exit 0
