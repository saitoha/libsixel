#!/bin/sh
# Update gdk-pixbuf loader cache if gdk-pixbuf-query-loaders is available.
# The script is intended to be called from installers and assumes DESTDIR is
# not set.

set -e

query_loader="$1"

if [ -z "$query_loader" ]; then
  echo "Warning: gdk-pixbuf-query-loaders path is empty; skip cache update" >&2
  exit 0
fi

echo "Updating gdk-pixbuf loader cache"
if "$query_loader" --update-cache; then
  echo "Updated gdk-pixbuf loader cache"
else
  echo "Warning: failed to update gdk-pixbuf loader cache" >&2
fi
