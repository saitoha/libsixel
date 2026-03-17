#!/usr/bin/env bash
#
# setup-buildtools.sh
#
# Usage:
#   ./setup-autotools.sh [--m4=1.4.21 --autoconf=2.72 --automake=1.18.1 --libtool=2.5.4 --meson=1.8.3]
#
# Optional:
#   --prefix=DIR    Install root (default: $PWD/.local)
#   --clean         Remove previous builds
#   --verify        Check versions only (no build)
#
set -euo pipefail

# === Default configuration ===
PREFIX="$PWD/.local"
SRC_DIR="$PREFIX/src"
MESON_ENV="$PREFIX/meson-env"
MIRROR_BASE="https://ftp.jaist.ac.jp/pub/GNU"
CLEAN=0
VERIFY=0
PERL_BIN="${PERL_BIN:-$(command -v perl || true)}"

# === Read args or fallback to environment vars ===
M4_VER="${M4_VER:-1.4.21}"
AUTOCONF_VER="${AUTOCONF_VER:-2.72}"
AUTOMAKE_VER="${AUTOMAKE_VER:-1.18.1}"
LIBTOOL_VER="${LIBTOOL_VER:-2.5.4}"
MESON_VER="${MESON_VER:-1.8.3}"

timeout() {
    if [ $# -lt 2 ]; then
        echo "usage: timeout SECONDS command [args...]" >&2
        return 125
    fi

    timeout_duration=$1
    shift

    # 実行
    (
        if command -v setsid >/dev/null 2>&1; then
            setsid "$@" &
        else
            "$@" &
        fi

        cmd_pid=$!

        # watchdog
        (
            sleep "$timeout_duration"
            kill -TERM -"$cmd_pid" 2>/dev/null
            sleep 1
            kill -KILL -"$cmd_pid" 2>/dev/null
        ) &
        watchdog_pid=$!
        wait "$cmd_pid"
        cmd_status=$?
        kill "$watchdog_pid" 2>/dev/null
        wait "$watchdog_pid" 2>/dev/null
        case "$cmd_status" in
            137|143)
                exit 124
                ;;
            *)
                exit "$cmd_status"
                ;;
        esac
    )
    return $?
}

for arg in "$@"; do
  case $arg in
    --m4=*)       M4_VER="${arg#*=}" ;;
    --autoconf=*) AUTOCONF_VER="${arg#*=}" ;;
    --automake=*) AUTOMAKE_VER="${arg#*=}" ;;
    --libtool=*)  LIBTOOL_VER="${arg#*=}" ;;
    --meson=*)    MESON_VER="${arg#*=}" ;;
    --prefix=*)   PREFIX="${arg#*=}" ;;
    --clean)      CLEAN=1 ;;
    --verify)     VERIFY=1 ;;
    *) echo "Unknown option: $arg" >&2; exit 1 ;;
  esac
done

if [ -z "${PERL_BIN}" ] || [ ! -x "${PERL_BIN}" ]; then
  echo "❌ perl was not found. Please install perl or set PERL_BIN=/path/to/perl" >&2
  exit 1
fi

SRC_DIR="$PREFIX/src"
MESON_ENV="$PREFIX/meson-env"
mkdir -p "$PREFIX/bin" "$SRC_DIR"
export PATH="$PREFIX/bin:$PATH"

# === Downloader (curl preferred, wget fallback, 10s hard timeout) ===
fetch() {
  local url="$1"
  local file="${url##*/}"
  local timeout_sec=10

  echo "📡 Downloading: $file"
  if command -v curl >/dev/null 2>&1; then
    if [ -f "$file" ]; then
      echo "✔️  Already exists: $file"
    else
      timeout "$timeout_sec" curl -L --progress-bar -o "$file" "$url" || return 1
    fi
  elif command -v wget >/dev/null 2>&1; then
    timeout "$timeout_sec" wget -q --show-progress -nc "$url" || return 1
  else
    echo "❌ Neither curl nor wget found. Please install one of them." >&2
    exit 1
  fi
}

# === Mirror fallback with persistence ===
download_with_fallback() {
  local name=$1 ver=$2
  local tar="${name}-${ver}.tar.gz"
  local mirrors=(
    "$MIRROR_BASE"
    "https://mirrors.kernel.org/gnu"
    "https://mirror.csclub.uwaterloo.ca/gnu"
    "https://ftp.gnu.org/gnu"
    "https://ftpmirror.gnu.org"
  )

  for base in "${mirrors[@]}"; do
    local url="${base}/${name}/${tar}"
    echo "🌐 Trying $url"
    if fetch "$url"; then
      echo "✅ Source acquired from $base"
      MIRROR_BASE="$base"
      return 0
    else
      echo "⚠️  Failed from $base (timeout or error)"
    fi
  done
  echo "❌ All mirrors failed for $name $ver"
  exit 1
}

# === Verify-only mode ===
if [ "$VERIFY" -eq 1 ]; then
  echo "🔍 Checking versions in $PREFIX/bin"
  for tool in m4 autoconf automake libtool meson; do
    if command -v "$tool" >/dev/null 2>&1; then
      echo "  ✔️ $($tool --version | head -n1)"
    else
      echo "  ❌ $tool not found"
    fi
  done
  exit 0
fi

# === Clean old builds ===
if [ "$CLEAN" -eq 1 ]; then
  echo "🧹 Cleaning $PREFIX..."
  rm -rf "$SRC_DIR"/m4-* "$SRC_DIR"/autoconf-* "$SRC_DIR"/automake-* "$SRC_DIR"/libtool-* "$MESON_ENV"
  echo "✨ Clean complete."
fi

# === GNU installer helper ===
install_gnu() {
  local name=$1 ver=$2
  echo
  echo "🚀 Installing $name $ver"
  cd "$SRC_DIR"
  local tar="${name}-${ver}.tar.gz"

  echo "  🌀 Fetching source..."
  download_with_fallback "$name" "$ver"

  echo "  📦 Extracting..."
  tar xf "$tar"

  echo "  🔧 Configuring..."
  cd "${name}-${ver}"
  PERL="$PERL_BIN" ./configure --prefix="$PREFIX" >/dev/null

  echo "  🏗️  Building..."
  make -s -j"$(nproc)"

  echo "  💾 Installing..."
  make install >/dev/null

  echo "✅ Installed $($PREFIX/bin/${name} --version | head -n1)"
  cd ..
}

repair_perl_shebangs() {
  local bindir=$1
  local file
  local first

  for file in "$bindir"/*; do
    [ -f "$file" ] || continue
    [ -x "$file" ] || continue
    first=$(head -n 1 "$file" 2>/dev/null || true)
    case "$first" in
      '#!'*'/perl'*)
        if [ "$first" != "#!$PERL_BIN" ]; then
          "$PERL_BIN" -i -pe \
            'if ($. == 1 && /^#!.*\/perl(?:\s|$)/) { $_ = "#!'"$PERL_BIN"'\\n"; }' \
            "$file"
        fi
        ;;
    esac
  done
}

# === Install tools ===
[ -n "$M4_VER" ] && install_gnu "m4" "$M4_VER"
[ -n "$AUTOCONF_VER" ] && install_gnu "autoconf" "$AUTOCONF_VER"
[ -n "$AUTOMAKE_VER" ] && install_gnu "automake" "$AUTOMAKE_VER"
[ -n "$LIBTOOL_VER" ] && install_gnu "libtool" "$LIBTOOL_VER"
repair_perl_shebangs "$PREFIX/bin"

# === Meson installer ===
if [ -n "$MESON_VER" ]; then
  echo
  echo "🐍 Installing Meson $MESON_VER ..."
  python3 -m venv "$MESON_ENV"
  source "$MESON_ENV/bin/activate"
  echo "  📡 Installing meson via pip..."
  pip install --upgrade pip >/dev/null
  pip install "meson==${MESON_VER}" ninja >/dev/null
  deactivate
  ln -sf "$MESON_ENV/bin/meson" "$PREFIX/bin/meson"
  ln -sf "$MESON_ENV/bin/ninja" "$PREFIX/bin/ninja"
  echo "✅ Installed meson $($PREFIX/bin/meson --version)"
fi

# === Generate environment file ===
ENV_FILE="$PREFIX/env-autotools.sh"
cat > "$ENV_FILE" <<EOF
# Environment for custom autotools and meson
export PATH="$PREFIX/bin:\$PATH"
export ACLOCAL_PATH="$PREFIX/share/aclocal:\$ACLOCAL_PATH"
EOF

echo
echo "✨ All installations complete!"
echo
echo "🔧 To activate the environment, run:"
echo "  source \"$ENV_FILE\""
echo
echo "Then verify:"
echo "  m4 --version"
echo "  autoconf --version"
echo "  automake --version"
echo "  libtool --version"
echo "  meson --version"
