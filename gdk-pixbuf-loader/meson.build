loader_sources = [
  'sixel-pixbuf-loader.c',
]

loader_deps = gdk_pixbuf_loader_deps + [libsixel_dep]

shared_module('pixbufloader-sixel',
  loader_sources,
  include_directories: project_inc,
  dependencies: loader_deps,
  c_args: ['-DGDK_PIXBUF_ENABLE_BACKEND'],
  install: true,
  install_dir: gdk_pixbuf_loader_dir)

install_data('README.md',
  install_dir: join_paths(get_option('datadir'), 'doc', 'libsixel'))
