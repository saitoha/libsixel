#!/bin/sh
# Emit TAP for timeline logging component boundary checks.
# Behavioral session and clock assertions belong to policy-linked TAP tests.
# Policy: docs/cli/diagnostics.md

set -eu
source_files_helper=$(CDPATH='' cd -- "${0%/*}" && pwd -P)/list-source-files.sh

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

sh "$source_files_helper" "$src_root/src" "$src_root/include" "$src_root/tests" \
 --name '*.[ch]' --name '*.inc.c' --exec awk -v src_root="$src_root" '
function rel(path) {
    sub("^" src_root "/", "", path)
    return path
}
FILENAME != last_file {
    last_file = FILENAME
    path = rel(FILENAME)
}
# All rules below contain one of these literal fragments. Most source lines
# need none of the more expensive expressions, including generated tables.
index($0, "logger") == 0 && index($0, "timeline") == 0 &&
    index($0, "sleep") == 0 { next }
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
# A path-policy probe is allowed; an enabled method on the writer is not.
/sixel_timeline_writer_is_enabled|timeline_writer.*->[ \t]*enabled/ {
    print path ":" FNR ": timeline writer must not expose an enabled method"
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
FILENAME ~ /\/tests\/processing\/timeline\/.*\.c$/ &&
    /^timeline_verify_path_source\(void\)/ {
    print path ":" FNR \
        ": timeline helper names must be unique for amalgamation"
}
'

sh "$source_files_helper" "$src_root/amalgamation" "$src_root/wic" "$src_root/src" --name 'Makefile.am' --name 'Makefile.in' --name 'meson.build' --exec awk -v src_root="$src_root" '
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

awk -v src_root="$src_root" '
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
        print "src/encoder-core-encode.c:" FNR \
            ": serial_logger must be initialized before early cleanup paths"
    }
    in_func = 0
}
' "$src_root/src/encoder-core-encode.c"

awk '
/^sixel_timeline_logger_prepare_env\(/ {
    in_func = 1
    saw_env_gate = 0
}
in_func && /sixel_timeline_writer_log_path_is_enabled[ \t]*\(/ {
    saw_env_gate = 1
}
in_func && /sixel_allocator_new[ \t]*\(/ {
    if (!saw_env_gate) {
        print "src/timeline-logger.c:" FNR \
            ": prepare_env must skip allocator creation when logging is off"
    }
    in_func = 0
}
' "$src_root/src/timeline-logger.c"

awk '
FNR == 1 { in_func = 0 }
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
        print FILENAME ":" FNR \
            ": log_path must be zeroed before early cleanup paths"
    }
}
in_func && /sixel_mutex_init[ \t]*\(/ {
    if (!saw_log_path_copy) {
        print FILENAME ":" FNR \
            ": SIXEL_LOG_PATH must be copied before writer use"
    }
    in_func = 0
}
' "$src_root"/tests/frame/timeline/*.c

awk '
FNR == 1 { in_func = 0 }
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
        print FILENAME ":" FNR \
            ": log_path must be zeroed before early cleanup paths"
    }
    if (!saw_log_path_copy) {
        print FILENAME ":" FNR \
            ": SIXEL_LOG_PATH must be copied before writer use"
    }
    in_func = 0
}
' "$src_root"/tests/frame/timeline/*.c

# Environment setup and the single native producer boundary apply to the
# whole timeline suite. Behavioral JSONL assertions execute in the TAP tests.
awk '
/--env[ \t]+SIXEL_LOG_PATH=/ {
    print FILENAME ":" FNR ": timeline tests must not rely on test_runner --env"
}
/timeline\/[^"[:space:]]*_verify/ {
    print FILENAME ":" FNR ": timeline TAP must not launch a native verifier"
}
' "$src_root"/tests/frame/timeline/*.t

awk '
 /timeline\/[^"[:space:]]*_verify/ {
    print "tests/test_runner.c:" FNR \
        ": timeline verifier entry points must not be restored"
}
' "$src_root/tests/test_runner.c"

awk '
    FNR == 1 { in_func = 0 }
    /^timeline_parallel_encode_decode\(void\)/ {
        in_func = 1
        producer_file = FILENAME
    }
    in_func && /timeline_flush_writer[ \t]*\([ \t]*log_path[ \t]*\)/ {
        saw_flush = 1
    }
    in_func && /timeline_verify_jsonl[ \t]*\([ \t]*log_path[ \t]*\)/ {
        saw_inline_verify = 1
    }
    in_func && /^#endif/ {
        in_func = 0
    }
    END {
        if (!saw_flush) {
            print producer_file \
                ": producer must flush JSONL before returning success"
        }
        if (saw_inline_verify) {
            print producer_file \
                ": producer must leave JSONL verification to TAP shell"
        }
    }
' "$src_root"/tests/frame/timeline/*.c

awk '
    FNR == 1 { in_func = 0 }
    /^timeline_clock_origin_is_shared\(void\)/ {
        in_func = 1
        producer_file = FILENAME
    }
    in_func && /writer->[ \t]*vtbl->[ \t]*flush[ \t]*\([ \t]*writer[ \t]*\)/ {
        saw_flush = 1
    }
    in_func && /timeline_read_clock_samples[ \t]*\([ \t]*log_path[ \t]*,/ {
        saw_inline_verify = 1
    }
    in_func && /return[ \t]+success[ \t]*;/ {
        in_func = 0
    }
    END {
        if (!saw_flush) {
            print producer_file \
                ": producer must flush JSONL before returning success"
        }
        if (saw_inline_verify) {
            print producer_file \
                ": producer must leave clock JSONL verification to TAP shell"
        }
    }
' "$src_root"/tests/frame/timeline/*.c
# Tests borrowing SIXEL_LOG_PATH are native producers for shell verification.
# Inspect the complete file because helpers can precede the getenv call.
awk '
/sixel_compat_getenv[ \t]*\("SIXEL_LOG_PATH"\)/ { producer[FILENAME] = 1 }
/sixel_compat_fopen[ \t]*\(|fopen[ \t]*\(|fgets[ \t]*\(/ {
    reader[FILENAME] = FNR
}
END {
    for (file in producer) {
        if (file in reader) {
            print file ":" reader[file] ": native timeline producer must not read JSONL"
        }
    }
}
' "$src_root"/tests/frame/timeline/*.c
} >>"$report"

if test -s "$report"; then
    echo "not ok 1 - timeline logging stays componentized"
    sed 's/^/# /' "$report"
    exit 1
fi

echo "ok 1 - timeline logging stays componentized"
exit 0
