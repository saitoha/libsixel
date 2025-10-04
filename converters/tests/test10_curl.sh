#!/bin/sh
set -eu

. "$(dirname "$0")/lib.sh"

require_img2sixel

if [ "${HAVE_CURL:-0}" != "1" ]; then
  skip 'curl support disabled'
fi

printf '[test10] curl\n'

expect_failure run_img2sixel file:///test
expect_failure run_img2sixel https:///test

snake_file="$TOP_SRC_DIR/images/snake.jpg"
case "$snake_file" in
  /*)
    img2sixel "file://$snake_file" >/dev/null
    ;;
  *)
    img2sixel "file://$(pwd)/$snake_file" >/dev/null
    ;;
esac

snake_sixel=$TMP_DIR/snake.sixel
img2sixel "$TOP_SRC_DIR/images/snake.jpg" >"$snake_sixel"

if command -v openssl >/dev/null 2>&1 && [ -n "${PYTHON_BIN:-}" ]; then
  server_key=$TMP_DIR/server.key
  server_crt=$TMP_DIR/server.crt
  server_py=$TMP_DIR/server.py

  openssl genrsa | openssl rsa >"$server_key"
  openssl req -new -key "$server_key" -subj "/CN=localhost" |
    openssl x509 -req -signkey "$server_key" >"$server_crt"
  {
    echo "import BaseHTTPServer as bs, SimpleHTTPServer as ss, ssl"
    echo "httpd = bs.HTTPServer(('localhost', 4443), ss.SimpleHTTPRequestHandler)"
    echo "httpd.socket = ssl.wrap_socket(httpd.socket, certfile='server.crt', keyfile='server.key', server_side=True)"
    echo "httpd.handle_request()"
    echo "httpd.handle_request()"
  } >"$server_py"
  (
    cd "$TMP_DIR" || exit 1
    "$PYTHON_BIN" "$server_py" >/dev/null 2>&1
  ) &
  server_pid=$!
  sleep 1
  if run_img2sixel 'https://localhost:4443/snake.sixel'; then
    kill "$server_pid" 2>/dev/null || true
    skip 'HTTPS fetch unexpectedly succeeded'
  fi
  sleep 1
  img2sixel -k 'https://localhost:4443/snake.sixel' >/dev/null
  wait "$server_pid" 2>/dev/null || true
fi
