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

common_c_args = ['-DSIXEL_DISABLE_EXT_LIBS=1', '-DHAVE_CONFIG_H']
common_objc_args = ['-fobjc-arc', '-fmodules']
extension_objc_args = common_objc_args + ['-fapplication-extension']

quicklook_core = static_library(
  'sixelquicklookcore',
  ['../src/status.c', '../src/allocator.c', '../src/fromsixel.c'],
  include_directories: [project_inc, include_directories('../src')],
  c_args: common_c_args,
  install: false)

preview = executable(
  'SixelPreview',
  ['Sources/SixelPreviewMain.m', 'Sources/SixelPreviewProvider.m', 'Sources/SixelQuickLookShared.m'],
  include_directories: common_includes,
  objc_args: extension_objc_args,
  c_args: common_c_args,
  link_with: quicklook_core,
  dependencies: quicklook_extension_frameworks,
  install: false)

thumbnail = executable(
  'SixelThumbnail',
  ['Sources/SixelThumbnailMain.m', 'Sources/SixelThumbnailProvider.m', 'Sources/SixelQuickLookShared.m'],
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
  ['Sources/SixelPreviewBridge.m', 'Sources/SixelQuickLookShared.m'],
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
  install: true,
  install_dir: join_paths(get_option('libexecdir'), 'libsixel'))

install_subdir('products/SixelQuickLookHost.app',
  install_dir: join_paths(get_option('libexecdir'), 'libsixel'),
  install_mode: 'rwxr-xr-x')
install_subdir('products/SixelPreviewBridge.app',
  install_dir: join_paths(get_option('libexecdir'), 'libsixel'),
  install_mode: 'rwxr-xr-x')

quicklook_installer = find_program(join_paths(meson.source_root(), 'tools', 'quicklook-extension.bash'))

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
