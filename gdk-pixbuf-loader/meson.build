loader_sources = [
  'sixel-pixbuf-loader.c',
]

loader_deps = gdk_pixbuf_loader_deps + [libsixel_dep]

pixbufloader_sixel = shared_module('pixbufloader-sixel',
  loader_sources,
  include_directories: project_inc,
  dependencies: loader_deps,
  c_args: ['-DGDK_PIXBUF_ENABLE_BACKEND'],
  build_by_default: true,
  install: true,
  install_dir: gdk_pixbuf_loader_dir)

install_data('README.md',
  install_dir: join_paths(get_option('datadir'), 'doc', 'libsixel'))
