#!/bin/sh
# Regenerate or verify the deterministic decoding-pipeline figures.

set -eu

TOP_SRCDIR=${TOP_SRCDIR-${0%/*}/..}
TOP_SRCDIR=$(CDPATH='' cd -- "${TOP_SRCDIR}" && pwd -P)
PYTHON=${PYTHON-python3}

exec "${PYTHON}" \
    "${TOP_SRCDIR}/tools/plot_decoding_pipeline_figures.py" "$@"
