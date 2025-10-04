#!/bin/sh
set -eu

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)

CONFIG_SH=${TEST_CONFIG_SH:-}
if [ -z "$CONFIG_SH" ]; then
  CONFIG_SH="$SCRIPT_DIR/config.sh"
fi

if [ -f "$CONFIG_SH" ]; then
  # shellcheck disable=SC1090
  . "$CONFIG_SH"
elif [ -n "${TEST_CONFIG_SH:-}" ]; then
  echo "ERROR: config file not found: $CONFIG_SH" >&2
  exit 1
fi

if [ -z "${TEST_BUILD_DIR:-}" ]; then
  echo 'ERROR: TEST_BUILD_DIR is not set' >&2
  exit 1
fi

TEST_SRC_DIR=${TEST_SRC_DIR:-$(cd "$SCRIPT_DIR/.." && pwd)}
TEST_TOP_SRC_DIR=${TEST_TOP_SRC_DIR:-$(cd "$TEST_SRC_DIR/.." && pwd)}

scripts=$(find "$SCRIPT_DIR" -maxdepth 1 -name 'test*.sh' -type f | sort)

[ -n "$scripts" ] || {
  echo 'ERROR: no tests found' >&2
  exit 1
}

status=0
echo '[start]'
for script in $scripts; do
  set +e
  env \
    TEST_BUILD_DIR="$TEST_BUILD_DIR" \
    TEST_SRC_DIR="$TEST_SRC_DIR" \
    TEST_TOP_SRC_DIR="$TEST_TOP_SRC_DIR" \
    HAVE_PNG="${HAVE_PNG:-}" \
    HAVE_CURL="${HAVE_CURL:-}" \
    WANT_SIXEL2PNG="${WANT_SIXEL2PNG:-}" \
    WINE="${WINE:-}" \
    WINEEXT="${WINEEXT:-}" \
    "${script}"
  script_status=$?
  set -e
  if [ "$script_status" -eq 0 ]; then
    echo
    continue
  fi
  if [ "$script_status" -eq 77 ]; then
    echo "(skip) $(basename "$script")"
    echo
    continue
  fi
  status=$script_status
  break
done

if [ "$status" -eq 0 ]; then
  echo '[succeeded]'
fi

exit $status
