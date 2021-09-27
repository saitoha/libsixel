project('libsixel', ['c'], version: '1.10.2', license: 'MIT', default_options: ['buildtype=release', 'c_std=c99', 'warning_level=3'])

datadir = get_option('datadir')
if (get_option('bashcompletiondir') == '')
  bashcompletiondir = '@0@/bash-completion/completions'.format(datadir)
else
  bashcompletiondir = get_option('bashcompletiondir')
endif

if (get_option('zshcompletiondir') == '')
  zshcompletiondir = '@0@/zsh/site-functions'.format(datadir)
else
  zshcompletiondir = get_option('zshcompletiondir')
endif

libsixel_deps = []

cc = meson.get_compiler('c')

cc.has_function('unreachable')

conf_data = configuration_data({'_POSIX_C_SOURCE': '199309L'})

if cc.compiles('''
    #pragma GCC diagnostic push
    #pragma GCC diagnostic pop
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    __attribute__((deprecated)) void test(void) {}
''')
  conf_data.set('HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS', true)
endif

c_args = [
  '-Wformat=2',
  '-Wsign-conversion',
  '-Wuninitialized',
  '-Wstrict-overflow',
  '-Wshadow',
  '-Wdouble-promotion',
  '-Wswitch-default',
  '-Wunused-function',
  '-Wunused-but-set-variable',
  '-Bsymbolic',
]

foreach a : c_args
  cc.has_argument(a)
endforeach

add_global_arguments(c_args, language: 'c')

if get_option('gcov').enabled()
  add_global_arguments('--coverage', language: 'c')
  add_global_link_arguments('--coverage', language: 'c')
endif


libm_found = cc.find_library('m')
libsixel_deps += [libm_found]

needed_headers = [
  'assert.h',
  'math.h',
  'ctype.h',
  'memory.h',
  'string.h',
  'unistd.h',
  'stdint.h',
  'getopt.h',
  'sys/types.h',
  'sys/stat.h',
  'fcntl.h',
  'errno.h',
  'limits.h',
  'sys/time.h',
  'time.h',
  'signal.h',
  'sys/select.h',
  'sys/signal.h',
  'termios.h',
  'sys/ioctl.h',
  'inttypes.h'
]

cc.has_function('malloc')
cc.has_function('realloc')
cc.has_function('memset')
have_getopt_long = cc.has_function('getopt_long')

foreach a : needed_headers
  assert(cc.has_header(a), 'Needed header not found')
endforeach

gdkpixbuf2_found = dependency('gdk-pixbuf-2.0', required: get_option('gdk-pixbuf2'))
gd_found = cc.find_library('gd', required: get_option('gd'))
curl_found = dependency('libcurl', required: get_option('libcurl'))
jpeg_found = dependency('libjpeg', required: get_option('jpeg'))
png_found = dependency('libpng', required: get_option('png'))
pymod = import('python')
python2_installation = pymod.find_installation('python2', required: get_option('python2'))

if get_option('libcurl').enabled() and curl_found.found()
  conf_data.set('HAVE_LIBCURL', true)
  libsixel_deps += [curl_found]
endif

if get_option('jpeg').enabled() and jpeg_found
  conf_data.set('HAVE_JPEG', true)
  libsixel_deps += [jpeg_found]
endif

if get_option('png').enabled() and png_found
  conf_data.set('HAVE_PNG', true)
  libsixel_deps += [png_found]
endif

loaders = ['stb-image']

if get_option('gd').enabled() and gd_found.found()
  conf_data.set('HAVE_GD', true)
  libsixel_deps += [gd_found]
  loaders += ['gd']

  gd_funcs = [
    'gdImageCreateFromGifPtr', 'gdImageCreateFromPngPtr',
    'gdImageCreateFromBmpPtr', 'gdImageCreateFromJpegPtrEx',
    'gdImageCreateFromJpegPtr', 'gdImageCreateFromWBMPPtr',
    'gdImageCreateFromTiffPtr', 'gdImageCreateFromGd2Ptr',
    'gdImagePaletteToTrueColor'
  ]

  foreach f : gd_funcs
    if cc.has_function(f, dependencies: gd_found)
      conf_data.set('HAVE_DECL_@0@'.format(f.to_upper()), true)
    endif
  endforeach
endif

if get_option('gdk-pixbuf2').enabled() and gdkpixbuf2_found.found()
  conf_data.set('HAVE_GDK_PIXBUF2', true)
  libsixel_deps += [gdkpixbuf2_found]
  loaders += ['gdk-pixbuf2']
endif

if have_getopt_long
  conf_data.set('HAVE_GETOPT_LONG', true)
endif

configure_file(output: 'config.h', configuration: conf_data)
inc_config = include_directories('.')

subdir('include')
subdir('src')
subdir('converters')
subdir('tools')

if get_option('python2').enabled() and python2_installation.found()
  subdir('python')
endif

warning('Perl, PHP and Ruby modules are available but not installed by Meson. If you want them, please refer to their individual installation directories for instructions after building libsixel.')

message('''

libsixel was configured as follows

       Install prefix      : @0@
       Loader component    : @1@
       libcurl integration : @2@
       Bash completion dir : @3@
       Zsh completion dir  : @4@
       python bindings     : @5@
       gcov integration    : @6@
       build type          : @7@
       tests               : @8@

  '''.format(
    get_option('prefix'),
    ' '.join(loaders),
    curl_found.found(),
    bashcompletiondir,
    zshcompletiondir,
    python2_installation.found().to_string(),
    get_option('gcov').enabled(),
    get_option('buildtype'),
    get_option('tests').enabled()
  )
)
