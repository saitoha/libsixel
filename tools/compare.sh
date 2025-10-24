#!/bin/sh

set -e

dir="$(dirname "${0}")"
src_topdir="${dir}/.."
in="${1}"

magick "${in}" -dither FloydSteinberg six:- |
"${src_topdir}"/tools/evaluate.sh "${in}" - imagemagick

converters/img2sixel -dlso2 -yserpentine -qfull -shistogram -Lrobinhood "${in}" |
"${src_topdir}"/tools/evaluate.sh "${in}" - libsixel

chafa -f sixel --dither diffusion "${in}" |
"${src_topdir}"/tools/evaluate.sh "${in}" - chafa
