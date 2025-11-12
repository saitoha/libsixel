#!/bin/bash

set -e

dir="$(dirname "${0}")"
src_topdir="${dir}/.."
in="${1}"
colors="${2-256}"

magick "${in}" \
    -dither FloydSteinberg \
    ${colors:+-colors "${colors}"} \
    six:- > imagemagick.six

converters/img2sixel "${in}" \
    -dlso2 -yserpentine -qfull -shistogram -Lcertlut \
    ${colors:+-p"${colors}"} \
    > libsixel.six

chafa "${in}" \
    -f sixel --dither diffusion \
    ${colors:+-c "${colors}"} \
    > chafa.six

"${src_topdir}"/tools/evaluate_sixel.sh "${in}" imagemagick.six
"${src_topdir}"/tools/evaluate_sixel.sh "${in}" libsixel.six
"${src_topdir}"/tools/evaluate_sixel.sh "${in}" chafa.six
