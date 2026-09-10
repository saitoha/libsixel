#!/bin/sh
# Regenerate or verify the deterministic encoder execution-map figures.

set -eu

TOP_SRCDIR=${TOP_SRCDIR-${0%/*}/..}
TOP_SRCDIR=$(CDPATH='' cd -- "${TOP_SRCDIR}" && pwd -P)
PYTHON=${PYTHON-python3}

exec "${PYTHON}" \
    "${TOP_SRCDIR}/tools/plot_encoder_execution_map_figures.py" "$@"
