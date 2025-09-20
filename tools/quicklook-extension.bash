#!/bin/bash
# Build and install the Quick Look preview/thumbnail extensions (AppKit view)
set -eu

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
RES_DIR="$ROOT/quicklook-extension/Resources"
ENTITLEMENTS="$RES_DIR/QuickLookExtension.entitlements"

MODE="install"
PRODUCT_DIR=""
REGISTER_TOOL=""
COLOR_YELLOW=""
COLOR_RESET=""
TARGET_HOME_ARG=""
CURRENT_USER=""
TARGET_HOME=""
TARGET_USER=""
TARGET_UID=""
LAUNCHCTL_ASUSER=""
# Timeout (seconds) for qlmanage priming to avoid hangs
QLMANAGE_TIMEOUT="${QLMANAGE_TIMEOUT:-15}"

# Path to Launch Services registration tool
LSREGISTER="/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister"
if [ ! -x "$LSREGISTER" ]; then
    LSREGISTER="$(command -v lsregister 2>/dev/null || printf '')"
fi

if command -v id >/dev/null 2>&1; then
    CURRENT_USER="$(id -un 2>/dev/null || printf '')"
fi
if [ -z "$CURRENT_USER" ] && [ -n "${USER:-}" ]; then
    CURRENT_USER="$USER"
fi

if command -v tput >/dev/null 2>&1 && [ -t 1 ]; then
    COLOR_YELLOW=$(tput setaf 3 || printf '')
    COLOR_RESET=$(tput sgr0 || printf '')
fi

usage() {
    echo "usage: $0 [install|uninstall] [--products-dir DIR] [--register-tool PATH] [--user-home PATH] [--no-prime]" >&2
    exit 1
}

set_target_user() {
    local user="$1"

    if [ -z "$user" ]; then
        return 1
    fi

    TARGET_USER="$user"

    if command -v id >/dev/null 2>&1; then
        TARGET_UID="$(id -u "$user" 2>/dev/null || printf '')"
    else
        TARGET_UID=""
    fi

    LAUNCHCTL_ASUSER=""

    return 0
}

resolve_user_home() {
    local candidate="$1"
    local resolved=""

    if [ -z "$candidate" ]; then
        return 1
    fi

    resolved=$(eval "printf '%s\n' \"~$candidate\"" 2>/dev/null || true)
    if [ -z "$resolved" ] || [ "$resolved" = "~$candidate" ]; then
        return 1
    fi

    if [ -d "$resolved" ]; then
        TARGET_HOME="$resolved"
        set_target_user "$candidate" || true
        return 0
    fi

    return 1
}

run_as_target_user() {
    if [ -n "$TARGET_USER" ] && [ -n "$CURRENT_USER" ] && [ "$TARGET_USER" != "$CURRENT_USER" ]; then
        if command -v sudo >/dev/null 2>&1; then
            sudo -H -u "$TARGET_USER" "$@"
        elif command -v su >/dev/null 2>&1; then
            local cmd=""
            for arg in "$@"; do
                cmd="$cmd $(printf '%q' "$arg")"
            done
            su "$TARGET_USER" -c "${cmd# }"
        else
            echo "warning: cannot switch to $TARGET_USER for $1" >&2
            "$@"
        fi
    else
        "$@"
    fi
}

check_launchctl_asuser() {
    if [ -n "$LAUNCHCTL_ASUSER" ]; then
        return
    fi

    if [ -z "$TARGET_UID" ]; then
        LAUNCHCTL_ASUSER="no"
        return
    fi

    if ! command -v launchctl >/dev/null 2>&1; then
        LAUNCHCTL_ASUSER="no"
        return
    fi

    if launchctl asuser "$TARGET_UID" true >/dev/null 2>&1; then
        LAUNCHCTL_ASUSER="yes"
    else
        LAUNCHCTL_ASUSER="no"
    fi
}

run_in_user_session() {
    check_launchctl_asuser

    if [ "$LAUNCHCTL_ASUSER" = "yes" ]; then
        launchctl asuser "$TARGET_UID" "$@"
    else
        run_as_target_user "$@"
    fi
}

while [ $# -gt 0 ]; do
    case "$1" in
        install|uninstall)
            MODE="$1"
            ;;
        --products-dir)
            shift
            [ $# -gt 0 ] || usage
            PRODUCT_DIR="$1"
            ;;
        --register-tool)
            shift
            [ $# -gt 0 ] || usage
            REGISTER_TOOL="$1"
            ;;
        --user-home)
            shift
            [ $# -gt 0 ] || usage
            TARGET_HOME_ARG="$1"
            ;;
        -h|--help)
            usage
            ;;
        *)
            usage
            ;;
    esac
    shift
done

if [ -z "$PRODUCT_DIR" ]; then
    PRODUCT_DIR="$ROOT/quicklook-extension/products"
fi

BUILD_ROOT="$(cd "$PRODUCT_DIR/.." 2>/dev/null && pwd || true)"
if [ -z "$REGISTER_TOOL" ] && [ -n "$BUILD_ROOT" ]; then
    REGISTER_TOOL="$BUILD_ROOT/register_sixel_preview"
fi

if [ -n "$TARGET_HOME_ARG" ]; then
    TARGET_HOME="$TARGET_HOME_ARG"
fi

if [ -z "$TARGET_HOME" ] && [ -n "${SUDO_USER:-}" ] && [ "${SUDO_USER:-}" != "root" ]; then
    resolve_user_home "$SUDO_USER" || true
fi

if [ -z "$TARGET_HOME" ] && [ -n "${LOGNAME:-}" ] && [ "${LOGNAME:-}" != "root" ]; then
    resolve_user_home "$LOGNAME" || true
fi

if [ -z "$TARGET_HOME" ] && [ -n "${USER:-}" ]; then
    resolve_user_home "$USER" || true
fi

if [ -z "$TARGET_HOME" ] && [ -n "${HOME:-}" ]; then
    TARGET_HOME="$HOME"
    if [ -z "$TARGET_USER" ]; then
        set_target_user "$CURRENT_USER" || true
    fi
fi

if [ -z "$TARGET_HOME" ]; then
    echo "error: unable to determine target home directory" >&2
    exit 1
fi

if [ ! -d "$TARGET_HOME" ]; then
    echo "error: target home directory $TARGET_HOME does not exist" >&2
    exit 1
fi

if [ -z "$TARGET_USER" ]; then
    if command -v stat >/dev/null 2>&1; then
        if stat -f %Su "$TARGET_HOME" >/dev/null 2>&1; then
            set_target_user "$(stat -f %Su "$TARGET_HOME" 2>/dev/null || printf '')" || true
        elif stat -c %U "$TARGET_HOME" >/dev/null 2>&1; then
            set_target_user "$(stat -c %U "$TARGET_HOME" 2>/dev/null || printf '')" || true
        fi
    fi
fi

if [ -z "$TARGET_USER" ] && [ -n "${SUDO_USER:-}" ]; then
    set_target_user "$SUDO_USER" || true
fi

if [ -z "$TARGET_USER" ]; then
    set_target_user "$CURRENT_USER" || true
fi

HOST_DEST="$TARGET_HOME/Applications/SixelQuickLookHost.app"
BRIDGE_DEST="$TARGET_HOME/Applications/SixelPreviewBridge.app"

HOST_APP="$PRODUCT_DIR/SixelQuickLookHost.app"
PREVIEW_APPEX="$HOST_APP/Contents/PlugIns/SixelPreview.appex"
THUMB_APPEX="$HOST_APP/Contents/PlugIns/SixelThumbnail.appex"
BRIDGE_APP="$PRODUCT_DIR/SixelPreviewBridge.app"
PREVIEW_DEST="$HOST_DEST/Contents/PlugIns/SixelPreview.appex"
THUMB_DEST="$HOST_DEST/Contents/PlugIns/SixelThumbnail.appex"

# Info.plist generation and @PACKAGE_VERSION@ substitution are
# handled by the build system (Autotools/Meson) from
# *.plist.in to *.plist. No extra replacements here.

clear_fileprovider_cache() {
    local fp_base="$TARGET_HOME/Library/Application Support/FileProvider"
    if [ ! -d "$fp_base" ]; then
        return
    fi

    echo "Clearing File Provider thumbnail cache"

    find "$fp_base" -type d \
        -path '*/File Provider Storage/thumbnails' \
        -exec rm -rf {} + 2>/dev/null || true
    find "$fp_base" -type d \
        -path '*/File Provider Storage/previews' \
        -exec rm -rf {} + 2>/dev/null || true

    if command -v fileproviderctl >/dev/null 2>&1; then
        run_in_user_session fileproviderctl domain reload com.apple.CloudDocs >/dev/null 2>&1 || true
    fi
}

case "$MODE" in
install)
    if [ ! -d "$HOST_APP" ] || [ ! -d "$BRIDGE_APP" ]; then
        echo "error: Quick Look products not found in $PRODUCT_DIR" >&2
        echo "       build the extensions first (e.g. make -C quicklook-extension or ninja quicklook-extension-bundles)" >&2
        exit 1
    fi

    if [ -z "$REGISTER_TOOL" ] || [ ! -x "$REGISTER_TOOL" ]; then
        echo "error: register_sixel_preview not found (expected at $REGISTER_TOOL)" >&2
        exit 1
    fi

    mkdir -p "$(dirname "$HOST_DEST")"
    mkdir -p "$(dirname "$BRIDGE_DEST")"

    echo "Installing to $HOST_DEST"
    rm -rf "$HOST_DEST"
    cp -R "$HOST_APP" "$HOST_DEST"

    echo "Installing preview bridge to $BRIDGE_DEST"
    rm -rf "$BRIDGE_DEST"
    cp -R "$BRIDGE_APP" "$BRIDGE_DEST"

    # Force-register apps with LaunchServices so bundle IDs are immediately known
    if [ -f "$ENTITLEMENTS" ]; then
        codesign --force --sign - --entitlements "$ENTITLEMENTS" "$HOST_DEST/Contents/PlugIns/SixelPreview.appex"
        codesign --force --sign - --entitlements "$ENTITLEMENTS" "$HOST_DEST/Contents/PlugIns/SixelThumbnail.appex"
    fi
    codesign --force --sign - "$HOST_DEST"
    codesign --force --sign - "$BRIDGE_DEST"

    # Ensure Launch Services points to the installed app paths, not build paths.
    if [ -n "$LSREGISTER" ] && [ -x "$LSREGISTER" ]; then
        # Unregister any prior registrations from the build directory
        run_in_user_session "$LSREGISTER" -u "$HOST_APP" >/dev/null 2>&1 || true
        run_in_user_session "$LSREGISTER" -u "$BRIDGE_APP" >/dev/null 2>&1 || true
        # Register the installed destinations
        run_in_user_session "$LSREGISTER" -f "$HOST_DEST" >/dev/null 2>&1 || true
        run_in_user_session "$LSREGISTER" -f "$BRIDGE_DEST" >/dev/null 2>&1 || true
    fi

    echo "Registering SIXEL handler"
    if ! run_in_user_session "$REGISTER_TOOL"; then
        echo "warning: failed to register SIXEL handler" >&2
    fi

    # Add the extensions
    run_in_user_session /usr/bin/pluginkit -a -vvv "$PREVIEW_DEST" || true
    run_in_user_session /usr/bin/pluginkit -a -vvv "$THUMB_DEST" || true

    # Refresh plugin database and caches first
    run_in_user_session qlmanage -r >/dev/null 2>&1 || true
    run_in_user_session qlmanage -r cache >/dev/null 2>&1 || true

    # Nudge Quick Look processes to pick up new extensions
    run_in_user_session killall QuickLookUIService >/dev/null 2>&1 || true
    run_in_user_session killall com.apple.quicklook.ThumbnailsAgent >/dev/null 2>&1 || true

    # Safer cache refresh: ask Quick Look to rebuild its cache for the user
    run_in_user_session qlmanage -r cache >/dev/null 2>&1 || true

    printf '%s%s%s\n' "$COLOR_YELLOW" "Quick Look caches cleared. Log out and back in to refresh Finder thumbnails." "$COLOR_RESET"

    echo "Done."
    ;;
uninstall)
    echo "Uninstalling Quick Look extensions"
    # Proactively unregister from Launch Services to avoid stale "Open With" entries
    if [ -n "$LSREGISTER" ] && [ -x "$LSREGISTER" ]; then
        run_in_user_session "$LSREGISTER" -u "$HOST_DEST" >/dev/null 2>&1 || true
        run_in_user_session "$LSREGISTER" -u "$BRIDGE_DEST" >/dev/null 2>&1 || true
        # Also unregister possible build-dir registrations left from development
        run_in_user_session "$LSREGISTER" -u "$HOST_APP" >/dev/null 2>&1 || true
        run_in_user_session "$LSREGISTER" -u "$BRIDGE_APP" >/dev/null 2>&1 || true
    fi
    run_in_user_session /usr/bin/pluginkit -r "$PREVIEW_DEST" >/dev/null 2>&1 || true
    run_in_user_session /usr/bin/pluginkit -r "$THUMB_DEST" >/dev/null 2>&1 || true

    for bundle_id in \
        com.saitoha.libsixel.quicklook.preview \
        com.saitoha.libsixel.quicklook.thumbnail
    do
        run_in_user_session /usr/bin/pluginkit -D "$bundle_id" >/dev/null 2>&1 || true
    done

    if [ -d "$HOST_DEST" ]; then
        echo "Removing $HOST_DEST"
        rm -rf "$HOST_DEST"
    fi

    if [ -d "$BRIDGE_DEST" ]; then
        echo "Removing $BRIDGE_DEST"
        rm -rf "$BRIDGE_DEST"
    fi

    # Be conservative on uninstall: ask Quick Look to clear its cache
    # instead of force-removing directories to avoid side effects.
    run_in_user_session qlmanage -r cache >/dev/null 2>&1 || true

    # Refresh plugin database and caches first
    run_in_user_session qlmanage -r >/dev/null 2>&1 || true
    run_in_user_session qlmanage -r cache >/dev/null 2>&1 || true

    # Nudge Quick Look processes to pick up new extensions
    run_in_user_session killall QuickLookUIService >/dev/null 2>&1 || true
    run_in_user_session killall com.apple.quicklook.ThumbnailsAgent >/dev/null 2>&1 || true

    printf '%s%s%s\n' "$COLOR_YELLOW" "Quick Look cache reset for user. Log out and back in to refresh Finder thumbnails." "$COLOR_RESET"

    echo "Uninstalled."
    ;;
*)
    echo "usage: $0 [install|uninstall]" >&2
    exit 1
    ;;
esac
