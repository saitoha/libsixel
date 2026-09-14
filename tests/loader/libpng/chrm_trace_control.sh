#!/bin/sh
# Invoke the trace-only fixture through the configured test runtime.
set -eux
# System shells on macOS remove DYLD variables. The existing harness keeps
# its intended path in this protected variable for child-shell restoration.
test -z "${LSO_RUNTIME_SHLIBPATH_VALUE-}" ||
    export "${RUNTIME_SHLIBPATH_VAR}=${LSO_RUNTIME_SHLIBPATH_VALUE}"
exec ${SIXEL_CHRM_TRACE_RUNTIME-} "${SIXEL_CHRM_TRACE_RUNNER}" \
    loader/0200_png_chrm_trace_control "${SIXEL_CHRM_TRACE_FAULT}" "$@"
