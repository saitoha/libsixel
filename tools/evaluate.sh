#!/bin/sh

set -e

original="${1}"
[ -n "${original}" ]

sixel="${2--}"
[ -n "${sixel}" ]

tag="${3:-$(basename ${original})}"

src_topdir="$(dirname "${0}")/.."

"${src_topdir}"/converters/sixel2png ${sixel} |
"${src_topdir}"/tools/evaluate.py --ref "${original}" --prefix=evaluate-"${tag}"

