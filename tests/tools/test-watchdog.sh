#!/bin/sh
#
# SPDX-License-Identifier: MIT
# Watchdog for test processes tracked via pid files.

set -eu

usage() {
  cat <<'USAGE' >&2
Usage:
  test-watchdog.sh ensure --state-dir DIR --pid-file FILE [--log-file FILE]
  test-watchdog.sh stop --state-dir DIR --pid-file FILE [--log-file FILE]
  test-watchdog.sh daemon --state-dir DIR --pid-file FILE [--log-file FILE]
USAGE
  exit 2
}

state_dir=
watchdog_pid_file=
log_file=
interval=${SIXEL_WATCHDOG_INTERVAL:-5}
timeout_limit=${SIXEL_WATCHDOG_TIMEOUT:-300}
grace_period=${SIXEL_WATCHDOG_GRACE:-10}
stop_requested=no

log_msg() {
  if [ -n "$log_file" ]; then
    printf '%s\n' "$*" >>"$log_file"
  fi
}

parse_args() {
  mode=$1
  shift
  while [ $# -gt 0 ]; do
    case "$1" in
      --state-dir) state_dir=$2; shift ;;
      --pid-file) watchdog_pid_file=$2; shift ;;
      --log-file) log_file=$2; shift ;;
      *) usage ;;
    esac
    shift
  done

  if [ -z "$state_dir" ] || [ -z "$watchdog_pid_file" ]; then
    usage
  fi
}

read_field() {
  field=$1
  file=$2
  awk -F '=' "\$1 == \"$field\" {print \$2}" "$file" 2>/dev/null | head -n 1
}

kill_safely() {
  target_pid=$1
  recorded_ppid=$2

  if ! kill -0 "$target_pid" 2>/dev/null; then
    return
  fi

  current_ppid=$(ps -o ppid= -p "$target_pid" 2>/dev/null | tr -d ' ')
  if [ -n "$recorded_ppid" ] && [ -n "$current_ppid" ] && \
     [ "$current_ppid" != "$recorded_ppid" ]; then
    return
  fi

  kill -TERM "$target_pid" 2>/dev/null || true
  sleep "$grace_period"
  kill -KILL "$target_pid" 2>/dev/null || true
}

remove_pid_file() {
  file=$1
  rm -f "$file"
}

handle_pid_file() {
  file=$1
  forced=$2

  pid_value=$(read_field pid "$file")
  ppid_value=$(read_field ppid "$file")
  started_value=$(read_field started_at "$file")

  if [ -z "$pid_value" ]; then
    remove_pid_file "$file"
    return
  fi

  if ! kill -0 "$pid_value" 2>/dev/null; then
    remove_pid_file "$file"
    return
  fi

  if [ "$forced" = "yes" ]; then
    kill_safely "$pid_value" "$ppid_value"
    remove_pid_file "$file"
    return
  fi

  now=$(date +%s 2>/dev/null || printf '%s' "$timeout_limit")
  if [ -n "$started_value" ]; then
    age=$((now - started_value))
    if [ "$age" -gt "$timeout_limit" ]; then
      log_msg "timeout ${pid_value} age=${age}s file=${file}"
      kill_safely "$pid_value" "$ppid_value"
      remove_pid_file "$file"
      return
    fi
  fi

  current_ppid=$(ps -o ppid= -p "$pid_value" 2>/dev/null | tr -d ' ')
  if [ -n "$ppid_value" ] && [ -n "$current_ppid" ] && \
     [ "$ppid_value" != "$current_ppid" ]; then
    remove_pid_file "$file"
    return
  fi
}

scan_pid_files() {
  if [ ! -d "$state_dir" ]; then
    return
  fi

  for file in "$state_dir"/*.pid; do
    [ -e "$file" ] || continue
    handle_pid_file "$file" "$1"
  done
}

stop_loop() {
  stop_requested=yes
}

daemon_loop() {
  trap 'stop_loop' INT TERM HUP

  while :; do
    scan_pid_files "no"
    if [ "$stop_requested" = "yes" ]; then
      scan_pid_files "yes"
      exit 0
    fi
    sleep "$interval"
  done
}

ensure_daemon() {
  mkdir -p "$state_dir"

  if [ -f "$watchdog_pid_file" ]; then
    existing_pid=$(cat "$watchdog_pid_file" 2>/dev/null || true)
    if [ -n "$existing_pid" ] && kill -0 "$existing_pid" 2>/dev/null; then
      return 0
    fi
  fi

  "$0" daemon --state-dir "$state_dir" --pid-file "$watchdog_pid_file" \
    ${log_file:+--log-file "$log_file"} &
  echo $! >"$watchdog_pid_file"
}

stop_daemon() {
  if [ ! -f "$watchdog_pid_file" ]; then
    return
  fi

  target=$(cat "$watchdog_pid_file" 2>/dev/null || true)
  if [ -n "$target" ] && kill -0 "$target" 2>/dev/null; then
    kill -INT "$target" 2>/dev/null || true
    wait "$target" 2>/dev/null || true
  fi

  rm -f "$watchdog_pid_file"
}

if [ $# -lt 1 ]; then
  usage
fi

mode=$1
shift
parse_args "$mode" "$@"

case "$mode" in
  ensure) ensure_daemon ;;
  stop) stop_daemon ;;
  daemon) daemon_loop ;;
  *) usage ;;
 esac
