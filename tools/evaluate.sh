#!/bin/sh

set -e

original="${1}"
[ -n "${original}" ]

sixel="${2--}"
[ -n "${sixel}" ]

dir="$(dirname "${0}")"
src_topdir="${dir}/.."
tag="$(basename ${original})"

"${src_topdir}"/converters/sixel2png ${sixel} |
"${src_topdir}"/tools/evaluate.py --ref "${original}" --prefix=evaluate-"${tag}"

