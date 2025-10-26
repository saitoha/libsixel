#!/usr/bin/env bash

set -euo pipefail

# NOTE: The MSVC archiver expects the command line in the form below.
#
#             lib [flags] -OUT:<archive> <object> ...
#             ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
#             |_______________________________|
#                             |
#                     ASCII wiring diagram :)
#
# Automake invokes "ar" as:
#
#             ar <flags> <archive> <object> ...
#
# This helper aligns both worlds by peeling off any leading flag tokens and
# forwarding the remaining arguments to "lib" while preserving the archive
# name.  Without this the first flag (e.g. "cr") was misread as the output
# file, resulting in link failures.

while (($#)); do
    case "$1" in
        -*)
            shift
            ;;
        [cdmpqrstuvx]*)
            shift
            ;;
        *)
            out="$1"
            shift
            break
            ;;
    esac
done

if [[ -z "${out:-}" ]]; then
    echo "ar-msvc.sh: missing archive name" >&2
    exit 1
fi

exec lib -nologo "-OUT:${out}" "$@"
