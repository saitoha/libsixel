#!/bin/sh
# Convert metadata into a space-delimited test list.
# Supported mode:
#   tests -> scan the tests source tree and print runnable test scripts

set -eu

if [ "$#" -lt 2 ] || [ "$#" -gt 4 ]; then
    echo "Usage: $0 <tests> <tests-dir> [include|skip] [space|newline|make]" >&2
    exit 1
fi

mode=$1
tests_dir=$2
ruby_tests_mode=${3:-include}
output_mode=${4:-space}

case "$ruby_tests_mode" in
    include|skip)
        ;;
    *)
        echo "Unknown ruby-tests mode: $ruby_tests_mode" >&2
        exit 1
        ;;
esac

case "$output_mode" in
    space|newline|make)
        ;;
    *)
        echo "Unknown output mode: $output_mode" >&2
        exit 1
        ;;
esac

if [ ! -d "$tests_dir" ]; then
    echo "Tests directory not found: $tests_dir" >&2
    exit 1
fi

case "$mode" in
    tests)
        ;;
    *)
        echo "Unknown mode: $mode" >&2
        exit 1
        ;;
esac

# Run the scan from tests_dir and emit normalized relative paths.
# This avoids platform-specific absolute path forms (for example C:/...)
# from leaking into Meson test names where ':' is deprecated.
(
    cd "$tests_dir"
    find . \
        \( \
            -path './.perl-test-venv' -o \
            -path './.php-test-venv' -o \
            -path './.python-test-venv' -o \
            -path './.ruby-test-venv' -o \
            -path './_artifacts' \
        \) -prune -o \
        -type f \( \
        -name '*.t' -o \
        -path './bindings/php/[0-9][0-9][0-9][0-9]_*.php' -o \
        -path './bindings/python/[0-9][0-9][0-9][0-9]_*.py' -o \
        -path './bindings/ruby/[0-9][0-9][0-9][0-9]_*.rb' -o \
        -path './bindings/perl/[0-9][0-9][0-9][0-9]_*.pl' \
        \) -print
) |
    LC_ALL=C sort |
    awk -v ruby_tests_mode="$ruby_tests_mode" \
        -v output_mode="$output_mode" '
        function emit_make_path(path) {
            prefix = "TESTS +="
            extra = length(path) + 1
            if (make_line_len == 0) {
                printf "%s %s", prefix, path
                make_line_len = length(prefix) + extra
            } else if (make_line_len + extra > 1800) {
                printf "\n%s %s", prefix, path
                make_line_len = length(prefix) + extra
            } else {
                printf " %s", path
                make_line_len += extra
            }
        }
        BEGIN {
            if (output_mode == "make") {
                printf "TESTS =\n"
            }
        }
        {
            path = $0
            sub("^\\./", "", path)
            if (path == "docs/consistency/0004_envvars_vs_help.t") {
                # This check is static and runs under the dedicated
                # staticcheck target instead of runtime test suites.
                next
            }
            if (path == "docs/consistency/0001_help_vs_man.t") {
                # This check is now covered by staticcheck.
                next
            }
            if (path == "docs/consistency/0002_man_vs_bash_completion.t") {
                # This check is now covered by staticcheck.
                next
            }
            if (path == "loader/builtin/1464_loader_builtin_psd_psdtools_blend_and_clipping_clip_weighted_deferred_solid_overlay_trace.t") {
                # This check validates static code contracts in src/frompsd.c.
                # Keep it under staticcheck to avoid dynamic check overhead.
                next
            }
            if (path == "loader/builtin/1578_loader_builtin_psd_psdtools_effects_stroke_composite_vector_stroke_adjust_deferred_trace.t") {
                # This check validates static code contracts in src/frompsd.c.
                # Keep it under staticcheck to avoid dynamic check overhead.
                next
            }
            if (path == "quant/palette/init/0073_kcenter_auto_perceptual_oklab_hybrid_preference.t") {
                # This check validates static code contracts in a C test source.
                # Keep it under staticcheck to avoid dynamic check overhead.
                next
            }
            if (ruby_tests_mode == "skip" &&
                path ~ /^bindings\/ruby\/[0-9][0-9][0-9][0-9]_.+\.rb$/) {
                next
            }
            if (output_mode == "make") {
                emit_make_path(path)
            } else if (output_mode == "newline") {
                printf "%s\n", path
            } else {
                printf "%s ", path
            }
        }
        END {
            if (output_mode == "make" && make_line_len != 0) {
                printf "\n"
            } else if (output_mode != "newline") {
                printf "\n"
            }
        }
    '
