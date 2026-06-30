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
# The Objective-C frontends expect the compat
# helpers that power strerror() and snprintf().
# Bundling compat_stub.c here mirrors the layout
# used by the Autotools build and prevents linker
# gaps when the static archive is reused.

srcs = files(
  '../src/status.c',
  '../src/allocator.c',
  '../src/fromsixel.c',
  '../src/timer.c',
  '../src/timeline-logger.c',
  '../src/timeline-writer.c',
  '../src/compat_stub.c',
  '../src/path.c',
)
srcs += [sixcells_h]

if conf.get('SIXEL_ENABLE_THREADS', 0) == 1
  srcs += [
    '../src/threading.c',
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
if shebang_enabled
  shebang_targets += [custom_target('SixelPreview_shebang',
    input: preview,
    output: 'SixelPreview.shebang.stamp',
    command: [shebang_helper, shebang_file, '@INPUT@'],
    depend_files: shebang_file,
    build_by_default: true)]
endif

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
if shebang_enabled
  shebang_targets += [custom_target('SixelThumbnail_shebang',
    input: thumbnail,
    output: 'SixelThumbnail.shebang.stamp',
    command: [shebang_helper, shebang_file, '@INPUT@'],
    depend_files: shebang_file,
    build_by_default: true)]
endif

host = executable(
  'SixelQuickLookHost',
  ['Sources/SixelQuickLookHost.m'],
  include_directories: common_includes,
  objc_args: common_objc_args,
  dependencies: quicklook_extension_frameworks,
  install: false)
if shebang_enabled
  shebang_targets += [custom_target('SixelQuickLookHost_shebang',
    input: host,
    output: 'SixelQuickLookHost.shebang.stamp',
    command: [shebang_helper, shebang_file, '@INPUT@'],
    depend_files: shebang_file,
    build_by_default: true)]
endif

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
if shebang_enabled
  shebang_targets += [custom_target('SixelPreviewBridge_shebang',
    input: bridge,
    output: 'SixelPreviewBridge.shebang.stamp',
    command: [shebang_helper, shebang_file, '@INPUT@'],
    depend_files: shebang_file,
    build_by_default: true)]
endif

register_tool = executable(
  'register_sixel_preview',
  ['../tools/register_sixel_preview.m'],
  include_directories: common_includes,
  objc_args: common_objc_args,
  dependencies: quicklook_extension_register_frameworks,
  install: true,
  install_dir: join_paths(get_option('libexecdir'), 'libsixel'))
if shebang_enabled
  shebang_targets += [custom_target('register_sixel_preview_shebang',
    input: register_tool,
    output: 'register_sixel_preview.shebang.stamp',
    command: [shebang_helper, shebang_file, '@INPUT@'],
    depend_files: shebang_file,
    build_by_default: true)]
endif

install_subdir('products/SixelQuickLookHost.app',
  install_dir: join_paths(get_option('libexecdir'), 'libsixel'),
  install_mode: 'rwxr-xr-x')
install_subdir('products/SixelPreviewBridge.app',
  install_dir: join_paths(get_option('libexecdir'), 'libsixel'),
  install_mode: 'rwxr-xr-x')

quicklook_installer = find_program(join_paths(meson.project_source_root(), 'tools', 'quicklook-extension.bash'))

run_target(
  'install-quicklook-extension',
  command: [quicklook_installer, 'install',
            '--products-dir', products_dir,
            '--register-tool', register_tool.full_path()],
  depends: [register_tool])

run_target(
  'uninstall-quicklook-extension',
  command: [quicklook_installer, 'uninstall',
            '--products-dir', products_dir,
            '--register-tool', register_tool.full_path()],
  depends: [register_tool])

# vim: set et ts=2:
