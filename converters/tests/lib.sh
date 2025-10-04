#!/bin/sh

fail() {
  echo "ERROR: $*" >&2
  exit 1
}

skip() {
  if [ "${1-}" != "" ]; then
    echo "SKIP: $1" >&2
  fi
  exit 77
}

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
: "${TEST_BUILD_DIR:?TEST_BUILD_DIR is not set}"
BUILD_DIR=$TEST_BUILD_DIR

if [ -n "${TEST_SRC_DIR:-}" ]; then
  SRC_DIR=$TEST_SRC_DIR
else
  SRC_DIR=$(cd "$SCRIPT_DIR/.." && pwd)
fi

if [ -n "${TEST_TOP_SRC_DIR:-}" ]; then
  TOP_SRC_DIR=$TEST_TOP_SRC_DIR
else
  TOP_SRC_DIR=$(cd "$SRC_DIR/.." && pwd)
fi

TMP_DIR=$BUILD_DIR/tmp
mkdir -p "$TMP_DIR"

WINE=${WINE:-}
WINEEXT=${WINEEXT:-}

if command -v python3 >/dev/null 2>&1; then
  PYTHON_BIN=python3
elif command -v python >/dev/null 2>&1; then
  PYTHON_BIN=python
else
  PYTHON_BIN=
fi

IMG2SIXEL_BIN=$BUILD_DIR/img2sixel$WINEEXT
SIXEL2PNG_BIN=$BUILD_DIR/sixel2png$WINEEXT

run_img2sixel() {
  if [ -n "$WINE" ]; then
    "$WINE" "$IMG2SIXEL_BIN" "$@"
    return $?
  fi
  "$IMG2SIXEL_BIN" "$@"
}

run_sixel2png() {
  if [ -n "$WINE" ]; then
    "$WINE" "$SIXEL2PNG_BIN" "$@"
    return $?
  fi
  "$SIXEL2PNG_BIN" "$@"
}

img2sixel() {
  run_img2sixel "$@"
}

sixel2png() {
  run_sixel2png "$@"
}

have_img2sixel() {
  [ -x "$IMG2SIXEL_BIN" ]
}

have_sixel2png() {
  [ -x "$SIXEL2PNG_BIN" ]
}

require_img2sixel() {
  have_img2sixel || skip "img2sixel not built"
}

require_sixel2png() {
  have_sixel2png || skip "sixel2png not built"
}

expect_failure() {
  set +e
  "$@"
  status=$?
  set -e
  if [ "$status" -eq 0 ]; then
    fail "expected failure: $*"
  fi
  return 0
}

expect_exit_in() {
  allowed=$1
  shift
  set +e
  "$@"
  status=$?
  set -e
  for code in $allowed; do
    if [ "$status" = "$code" ]; then
      return 0
    fi
  done
  fail "unexpected exit status $status: $*"
}
