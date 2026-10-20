# Keep assembly outputs intact; signing belongs to this separate producer.
quicklook_package = custom_target('quicklook-package',
  input: [quicklook_bundles, register_tool, package_tool,
          files('../Resources/QuickLookExtension.entitlements'),
          files('../package-preinstall'), files('../package-postinstall')],
  output: ['SixelQuickLook.pkg', 'payload-manifest.json',
           'package-evidence.json', 'work'],
  command: [package_tool, 'build', meson.current_build_dir(),
            '@INPUT0@', '@INPUT1@', '@INPUT2@', '@INPUT3@', '@INPUT4@',
            meson.project_version(), quicklook_package_version, '@INPUT5@', '@INPUT6@'],
  build_by_default: true,
  install: false)
