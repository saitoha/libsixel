# SPDX-License-Identifier: MIT

if not have_quicklook_extension
  error('quicklook-extension entered without required frameworks')
endif

products_dir = join_paths(meson.current_build_dir(), 'products')
resources_conf = configuration_data()
resources_conf.set('PACKAGE_VERSION', meson.project_version())

preview_plist = configure_file(
  input: 'Resources/Preview-Info.plist.in',
  output: 'Preview-Info.plist',
  configuration: resources_conf)
thumbnail_plist = configure_file(
  input: 'Resources/Thumbnail-Info.plist.in',
  output: 'Thumbnail-Info.plist',
  configuration: resources_conf)
host_plist = configure_file(
  input: 'Resources/Host-Info.plist.in',
  output: 'Host-Info.plist',
  configuration: resources_conf)
bridge_plist = configure_file(
  input: 'Resources/Bridge-Info.plist.in',
  output: 'Bridge-Info.plist',
  configuration: resources_conf)

common_includes = [
  project_inc,
  include_directories('Sources'),
  include_directories('..'),
  include_directories('../src'),
]

common_c_args = ['-DSIXEL_DISABLE_EXT_LIBS=1',
                 '-DSIXEL_STATUS_HAVE_TTY_HELPERS=0',
                 '-DHAVE_CONFIG_H']
# -fmodules is brittle with some sanitizer + Homebrew clang environments
# (modulemap/header-search-path incompatibilities). Keep ARC enabled but rely
# on textual framework headers for wider CI portability.
common_objc_args = ['-fobjc-arc']
extension_objc_args = common_objc_args + ['-fapplication-extension']

# +----------------------------------------------+
# |  Quick Look core aggregation                 |
# +----------------------------------------------+
# Keep the standalone decoder dependencies aligned with the Autotools
# archive. The CPU detector gates decoder SIMD, the registry owns thread
# parsing, and compat_stub.c supplies their portability helpers.

srcs = files(
  'Sources/SixelQuickLookStub.c',
  '../src/status.c',
  '../src/allocator.c',
  '../src/fromsixel.c',
  '../src/colorspace-hls.c',
  '../src/cpu.c',
  '../src/timer.c',
  '../src/timeline-logger.c',
  '../src/timeline-writer.c',
  '../src/compat_stub.c',
  '../src/options-registry.c',
  '../src/threading.c',
  '../src/path.c',
)
srcs += [sixcells_h]

if conf.get('SIXEL_ENABLE_THREADS', 0) == 1
  srcs += [
    '../src/threadpool.c',
    '../src/decoder-parallel.c',
  ]
endif

quicklook_core = static_library(
  'sixelquicklookcore',
  srcs,
  include_directories: [project_inc, include_directories('../src')],
  c_args: common_c_args + ['-DSIXEL_DECODE_PIXELS_NO_FAST4=1'],
  install: false)

preview = executable(
  'SixelPreview',
  [
    'Sources/SixelPreviewMain.m',
    'Sources/SixelPreviewProvider.m',
    'Sources/SixelQuickLookShared.m'
  ],
  include_directories: common_includes,
  objc_args: extension_objc_args,
  c_args: common_c_args,
  link_with: quicklook_core,
  dependencies: quicklook_extension_frameworks,
  install: false)

thumbnail = executable(
  'SixelThumbnail',
  [
    'Sources/SixelThumbnailMain.m',
    'Sources/SixelThumbnailProvider.m',
    'Sources/SixelQuickLookShared.m'
  ],
  include_directories: common_includes,
  objc_args: extension_objc_args,
  c_args: common_c_args,
  link_with: quicklook_core,
  dependencies: quicklook_extension_frameworks,
  install: false)

host = executable(
  'SixelQuickLookHost',
  ['Sources/SixelQuickLookHost.m'],
  include_directories: common_includes,
  objc_args: common_objc_args,
  dependencies: quicklook_extension_frameworks,
  install: false)

bridge = executable(
  'SixelPreviewBridge',
  [
    'Sources/SixelPreviewBridge.m',
    'Sources/SixelQuickLookShared.m'
  ],
  include_directories: common_includes,
  objc_args: common_objc_args,
  c_args: common_c_args,
  link_with: quicklook_core,
  dependencies: quicklook_extension_frameworks,
  install: false)

register_tool = executable(
  'register_sixel_preview',
  ['../tools/register_sixel_preview.m'],
  include_directories: common_includes,
  objc_args: common_objc_args,
  dependencies: quicklook_extension_register_frameworks,
  install: false)

package_tool = executable('sixel-quicklook-package',
  'Sources/SixelPackageTool.m', 'Sources/SixelPackageChecks.h',
  'Sources/SixelInstalledChecks.h', 'Sources/SixelUser.h',
  'Sources/SixelLifecycle.h', 'Sources/SixelLegacy.h',
  objc_args: common_objc_args,
  dependencies: dependency('appleframeworks', modules: ['Foundation', 'Security',
    'CoreServices', 'SystemConfiguration', 'AppKit']),
  link_args: ['-weak_framework', 'UniformTypeIdentifiers'],
  install: false)

# The producer owns both generated directories and their installation inputs.
# The input list is also the boundary for subsequent payload signing.
subdir('products')
alias_target('quicklook-extension-bundles', quicklook_bundles, register_tool)
subdir('package')

quicklook_installer = find_program(join_paths(meson.project_source_root(), 'tools', 'quicklook-extension.bash'))

quicklook_manage_args = [
  '--package-dir', meson.current_build_dir() / 'package',
  '--tool', package_tool.full_path(),
  '--datadir', get_option('prefix') / get_option('datadir')]
# Meson does not run custom install scripts for --dry-run by default.
meson.add_install_script(quicklook_installer, 'install', quicklook_manage_args)
run_target('install-quicklook-extension',
  command: [quicklook_installer, 'install', quicklook_manage_args],
  depends: [quicklook_package])
run_target('uninstall-quicklook-extension',
  command: [quicklook_installer, 'uninstall', quicklook_manage_args])


# vim: set et ts=2:

# Private helpers share product code only at the controlled OS boundary.
lifecycle_driver = executable('sixel-quicklook-lifecycle-driver',
  '../tests/extension/quicklook-lifecycle/private-driver.m',
  include_directories: include_directories('../tests/extension/quicklook-lifecycle'),
  objc_args: common_objc_args,
  dependencies: dependency('appleframeworks', modules: ['Foundation', 'Security',
    'CoreServices', 'SystemConfiguration', 'AppKit']),
  link_args: ['-weak_framework', 'UniformTypeIdentifiers'], install: false)
lifecycle_os = executable('sixel-quicklook-lifecycle-os',
  '../tests/extension/quicklook-lifecycle/private-os.m',
  objc_args: common_objc_args, dependencies: dependency('appleframeworks',
    modules: ['Foundation']), install: false)
lifecycle_audit = executable('sixel-quicklook-lifecycle-audit',
  '../tests/extension/quicklook-lifecycle/audit.m',
  objc_args: common_objc_args, dependencies: dependency('appleframeworks',
    modules: ['Foundation', 'Security']), install: false)

# Explicit rendering diagnostics never run during build or installcheck.
render_probe = executable('sixel-quicklook-render',
  'Sources/SixelRenderProbe.m', objc_args: common_objc_args,
  dependencies: dependency('appleframeworks', modules: ['Foundation',
    'CoreGraphics', 'ImageIO', 'QuickLookThumbnailing']), install: false)
