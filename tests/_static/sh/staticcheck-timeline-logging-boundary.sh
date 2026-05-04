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
/--env[ \t]+SIXEL_LOG_PATH=/ {
    print "tests/processing/timeline/" \
        "0002_timeline_parallel_encode_decode.t:" FNR \
        ": timeline tests must not rely on test_runner --env"
}
' "$src_root/tests/processing/timeline/0002_timeline_parallel_encode_decode.t"

	awk '
	/timeline\/0002_timeline_parallel_encode_decode_verify/ {
	    saw_verify = 1
	}
	/SIXEL_TEST_TIMELINE_JSONL="\$\{log_path\}"/ {
	    saw_verify_path_env = 1
	}
	/MSYS_NO_PATHCONV=1/ {
	    saw_no_pathconv += 1
	}
	/MSYS2_ARG_CONV_EXCL='\''\*'\''/ {
	    saw_arg_conv_excl += 1
	}
	END {
	    if (!saw_verify || !saw_verify_path_env) {
	        print "tests/processing/timeline/" \
	            "0002_timeline_parallel_encode_decode.t:" \
	            ": timeline TAP must pass JSONL path through test env"
	    }
	    if (saw_no_pathconv < 2 || saw_arg_conv_excl < 2) {
	        print "tests/processing/timeline/" \
	            "0002_timeline_parallel_encode_decode.t:" \
	            ": timeline TAP must disable MSYS path conversion"
	    }
	}
	' "$src_root/tests/processing/timeline/0002_timeline_parallel_encode_decode.t"

awk '
/--env[ \t]+SIXEL_LOG_PATH=/ {
    print "tests/processing/timeline/" \
        "0003_timeline_clock_origin.t:" FNR \
        ": timeline tests must not rely on test_runner --env"
}
' "$src_root/tests/processing/timeline/0003_timeline_clock_origin.t"

awk '
 /timeline\/000[23].*_verify/ {
    found += 1
}
END {
    if (found < 2) {
        print "tests/test_runner.c:" \
            ": timeline verifier entry points must stay in test_runner"
    }
}
' "$src_root/tests/test_runner.c"

	awk '
	/timeline\/0003_timeline_clock_origin_verify/ {
	    saw_verify = 1
	}
	/SIXEL_TEST_TIMELINE_JSONL="\$\{log_path\}"/ {
	    saw_verify_path_env = 1
	}
	/MSYS_NO_PATHCONV=1/ {
	    saw_no_pathconv += 1
	}
	/MSYS2_ARG_CONV_EXCL='\''\*'\''/ {
	    saw_arg_conv_excl += 1
	}
	END {
	    if (!saw_verify || !saw_verify_path_env) {
	        print "tests/processing/timeline/" \
	            "0003_timeline_clock_origin.t:" \
	            ": timeline TAP must pass JSONL path through test env"
	    }
	    if (saw_no_pathconv < 2 || saw_arg_conv_excl < 2) {
	        print "tests/processing/timeline/" \
	            "0003_timeline_clock_origin.t:" \
	            ": timeline TAP must disable MSYS path conversion"
	    }
	}
	' "$src_root/tests/processing/timeline/0003_timeline_clock_origin.t"

awk '
	/^timeline_parallel_encode_decode\(void\)/ {
	    in_func = 1
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
	        print "tests/processing/timeline/" \
	            "0002_timeline_parallel_encode_decode.c:" \
	            ": producer must flush JSONL before returning success"
	    }
	    if (saw_inline_verify) {
	        print "tests/processing/timeline/" \
	            "0002_timeline_parallel_encode_decode.c:" \
	            ": producer must leave JSONL verification to TAP verifier"
	    }
	}
	' "$src_root/tests/processing/timeline/0002_timeline_parallel_encode_decode.c"

	awk '
	/^timeline_clock_origin_is_shared\(void\)/ {
	    in_func = 1
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
	        print "tests/processing/timeline/" \
	            "0003_timeline_clock_origin.c:" \
	            ": producer must flush JSONL before returning success"
	    }
	    if (saw_inline_verify) {
	        print "tests/processing/timeline/" \
	            "0003_timeline_clock_origin.c:" \
	            ": producer must leave clock JSONL verification to TAP verifier"
	    }
	}
	' "$src_root/tests/processing/timeline/0003_timeline_clock_origin.c"
} >>"$report"

if test -s "$report"; then
    echo "not ok 1 - timeline logging stays componentized"
    sed 's/^/# /' "$report"
    exit 1
fi

echo "ok 1 - timeline logging stays componentized"
exit 0
