#!/bin/sh
# Read config.h and print shell-safe assignment lines for feature macros.
#
# Default output format:
#   KEY='value'
# Optional mode "pairs" prints tab-delimited key/value pairs for parsers.

set -eu

if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
    echo "Usage: $0 <config.h> [exports|pairs]" >&2
    exit 1
fi

config_header=$1
mode=${2:-exports}

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

awk -v output_mode="$mode" '
    /^#define [A-Z0-9_]+ 1$/ {
        key = $2
        value = "1"
        if (output_mode == "pairs") {
            printf "%s\t%s\n", key, value
        } else {
            printf "%s=%s ", key, value
        }
        next
    }
    /^\/\* #undef [A-Z0-9_]+ \*\/$/ {
        key = $3
        value = "0"
        if (output_mode == "pairs") {
            printf "%s\t%s\n", key, value
        } else {
            printf "%s=%s ", key, value
        }
    }
' "$config_header"
