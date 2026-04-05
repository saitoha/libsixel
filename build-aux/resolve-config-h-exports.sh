#!/bin/sh
# Read config.h and print shell-safe assignment lines for feature macros.
#
# Default output format:
#   KEY='value'
# Optional mode "pairs" prints tab-delimited key/value pairs for parsers.
# Optional third argument:
#   key-list file (one macro name per line, comments with '#')
# When provided, only listed macros are emitted.

set -eu

if [ "$#" -lt 1 ] || [ "$#" -gt 3 ]; then
    echo "Usage: $0 <config.h> [exports|pairs] [key-list-file]" >&2
    exit 1
fi

config_header=$1
mode=${2:-exports}
key_list_file=${3:-}

if [ ! -f "$config_header" ]; then
    exit 0
fi

case "$mode" in
    exports|pairs)
        ;;
    *)
        echo "Unknown mode: $mode" >&2
        exit 1
        ;;
esac

if [ -n "$key_list_file" ] && [ ! -f "$key_list_file" ]; then
    echo "Key list not found: $key_list_file" >&2
    exit 1
fi

awk -v output_mode="$mode" -v key_list_file="$key_list_file" '
    BEGIN {
        use_filter = 0
        if (key_list_file != "") {
            use_filter = 1
            while ((getline key_line < key_list_file) > 0) {
                sub(/\r$/, "", key_line)
                gsub(/^[ \t]+/, "", key_line)
                gsub(/[ \t]+$/, "", key_line)
                if (key_line == "" || key_line ~ /^#/) {
                    continue
                }
                include_key[key_line] = 1
            }
            close(key_list_file)
        }
    }
    function emit_macro(key, value) {
        if (use_filter && !(key in include_key)) {
            return
        }
        if (output_mode == "pairs") {
            printf "%s\t%s\n", key, value
        } else {
            printf "%s=%s ", key, value
        }
    }
    /^#define [A-Z0-9_]+ 1$/ {
        emit_macro($2, "1")
        next
    }
    /^\/\* #undef [A-Z0-9_]+ \*\/$/ {
        emit_macro($3, "0")
    }
' "$config_header"
