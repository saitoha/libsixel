#!/bin/sh
# Assemble unsigned build-tree bundles from completed executables and plists.
# Do not register, sign, or change any installed application here.
set -eu

test "$#" -eq 10 || {
    echo "usage: $0 HOST_APP BRIDGE_APP HOST PREVIEW THUMBNAIL BRIDGE HOST_PLIST PREVIEW_PLIST THUMBNAIL_PLIST BRIDGE_PLIST" >&2
    exit 1
}
host_app=$1
bridge_app=$2
shift 2

# Validate all inputs before replacing either output. Never leave an old stamp
# representing a successful assembly when a required copy has failed.
for input in "$@"; do
    test -f "$input" || {
        echo "missing Quick Look bundle input: $input" >&2
        exit 1
    }
done

# Recreate only the two build outputs, including stale signatures/resources.
# The products directory itself can contain a source-tree meson.build file.
rm -rf "$host_app" "$bridge_app"
install -d "$host_app/Contents/MacOS" "$host_app/Contents/Resources" \
    "$host_app/Contents/PlugIns/SixelPreview.appex/Contents/MacOS" \
    "$host_app/Contents/PlugIns/SixelThumbnail.appex/Contents/MacOS" \
    "$bridge_app/Contents/MacOS" "$bridge_app/Contents/Resources"
install -m 755 "$1" "$host_app/Contents/MacOS/SixelQuickLookHost"
install -m 755 "$2" "$host_app/Contents/PlugIns/SixelPreview.appex/Contents/MacOS/SixelPreview"
install -m 755 "$3" "$host_app/Contents/PlugIns/SixelThumbnail.appex/Contents/MacOS/SixelThumbnail"
install -m 755 "$4" "$bridge_app/Contents/MacOS/SixelPreviewBridge"
shift 4
install -m 644 "$1" "$host_app/Contents/Info.plist"
install -m 644 "$2" "$host_app/Contents/PlugIns/SixelPreview.appex/Contents/Info.plist"
install -m 644 "$3" "$host_app/Contents/PlugIns/SixelThumbnail.appex/Contents/Info.plist"
install -m 644 "$4" "$bridge_app/Contents/Info.plist"
# Directory outputs must be newer than every input for Ninja's next build.
touch "$host_app" "$bridge_app"
