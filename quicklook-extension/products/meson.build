# SPDX-License-Identifier: MIT
# Keep every copied executable and generated plist in the dependency graph.
# App directories are declared outputs, not assumed source-tree artifacts.
quicklook_bundles = custom_target('quicklook-bundles',
  input: [host, preview, thumbnail, bridge,
          host_plist, preview_plist, thumbnail_plist, bridge_plist],
  output: ['SixelQuickLookHost.app', 'SixelPreviewBridge.app'],
  command: [sh, files('../assemble-bundles.sh'), '@OUTPUT0@', '@OUTPUT1@',
            '@INPUT@'],
  build_by_default: true,
  install: false)
