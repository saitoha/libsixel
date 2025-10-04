#!/bin/sh
set -eu

. "$(dirname "$0")/lib.sh"

require_img2sixel

printf '[test5] DCS arguments handling\n'

for i in $(seq 0 10); do
  for j in $(seq 0 2); do
    img2sixel "$TOP_SRC_DIR/images/map8.png" | \
      sed "s/Pq/P${i};;${j}q/" | \
      img2sixel >/dev/null
  done
done
