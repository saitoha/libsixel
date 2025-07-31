inc_conf_data = configuration_data()

pkg_description = 'A lightweight, fast C implementation of DEC SIXEL graphics codec'
inc_conf_data.set('PACKAGE_VERSION', meson.project_version())
inc_conf_data.set('PACKAGE_DESCRIPTION', pkg_description)
LS_LT_CURRENT=1
LS_LT_REVISION=9
LS_LT_AGE=0
inc_conf_data.set('LS_LTVERSION', '@LS_LT_CURRENT@:@LS_LT_REVISION@:@LS_LT_AGE@')

configure_file(input : 'sixel.h.in',
               output : 'sixel.h',
               configuration : inc_conf_data,
               install: true,
               install_dir: get_option('includedir'))

inc = include_directories('.')
