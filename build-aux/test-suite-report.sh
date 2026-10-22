#!/bin/sh
# Aggregate metadata once, then assemble only requested diagnostic logs.
set -eu

awk_bin=$1
prefix=$2
shift 2
diagnostics=$prefix.diagnostics
summary=$prefix.summary
# Cleanup emits no report data, including on GNV's inherited output channels.
trap 'test ! -e "$diagnostics" || rm -f "$diagnostics" >/dev/null;
      test ! -e "$summary" || rm -f "$summary" >/dev/null' 0 1 2 3 15

"$awk_bin" -v suite=1 -v diagnostics_file="$diagnostics" \
    -f "${0%/*}/test-suite-report.awk" "$@" > "$summary"

# H records are literal headings; F records are literal file paths. Neither
# is shell source. Native GNV cat inherits this shell's redirection; awk's
# system() does not. Copying through cat retains the shell collector's native
# record semantics, including byte-exact incomplete lines on POSIX hosts.
while IFS= read -r record; do
    case $record in
        H*) printf '%s\n' "${record#H}" ;;
        F*) cat "${record#F}"; printf '\n' ;;
        *) echo 'test-suite-report: invalid diagnostic record' >&2; exit 1 ;;
    esac
done < "$diagnostics"
cat "$summary"
