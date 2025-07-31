tools_conf_data = configuration_data()
tools_conf_data.merge_from(inc_conf_data)

tools_conf_data.set('bindir', get_option('bindir'))
tools_conf_data.set('libdir', get_option('libdir'))
tools_conf_data.set('includedir', get_option('includedir'))
tools_conf_data.set('datadir', get_option('datadir'))
tools_conf_data.set('datarootdir', get_option('datadir'))
tools_conf_data.set('mandir', get_option('mandir'))
tools_conf_data.set('prefix', get_option('prefix'))
tools_conf_data.set('exec_prefix', get_option('bindir'))

configure_file(input : 'libsixel-config.in',
               output : 'libsixel-config',
               configuration : tools_conf_data,
               install: true,
               install_dir: get_option('bindir'),
               install_mode: 'rwxr-xr-x')
