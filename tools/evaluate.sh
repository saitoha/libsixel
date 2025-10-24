#!/bin/sh

set -e

original="${1}"
[ -n "${original}" ]

sixel="${2-/dev/stdin}"
[ -n "${sixel}" ]

dir="$(dirname "${0}")"
src_topdir="${dir}/.."
tag="$(basename ${sixel})"

"${src_topdir}"/converters/sixel2png ${sixel} |
"${src_topdir}"/tools/evaluate.py --ref "${original}" --prefix=evaluate-"${tag}"

