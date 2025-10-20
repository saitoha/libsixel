# SPDX-License-Identifier: MIT
#
# Copyright (c) 2025 libsixel developers. See `AUTHORS`.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy of
# this software and associated documentation files (the "Software"), to deal in
# the Software without restriction, including without limitation the rights to
# use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
# the Software, and to permit persons to whom the Software is furnished to do so,
# subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
# FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
# COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
# IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#
project('libsixel',
        'c',
        version: '1.8.7',
        license: 'MIT',
        default_options: [
            'buildtype=release',
            'c_std=c99',
            'warning_level=3'
        ])

host_system = host_machine.system()
cc = meson.get_compiler('c')
fs = import('fs')
have_objc = add_languages('objc', required: false, native: false)
if have_objc
  objc = meson.get_compiler('objc')
  message('Objective-C compiler found: ' + objc.get_id())
else
  message('Objective-C compiler not available, skipping ObjC parts')
endif

# configuration data
conf = configuration_data()
conf.set_quoted('PACKAGE', meson.project_name())
conf.set_quoted('PACKAGE_NAME', meson.project_name())
conf.set_quoted('PACKAGE_STRING',
                meson.project_name() + ' ' + meson.project_version())
conf.set_quoted('PACKAGE_VERSION', meson.project_version())
conf.set_quoted('PACKAGE_BUGREPORT', 'saitoha@me.com')
conf.set_quoted('PACKAGE_URL', '')
conf.set_quoted('VERSION', meson.project_version())

if get_option('gcov')
  add_project_arguments('--coverage', language: 'c')
  add_project_link_arguments('--coverage', language: 'c')
endif
if get_option('debug')
  conf.set('HAVE_DEBUG', 1)
endif
if get_option('tests')
  conf.set('HAVE_TESTS', 1)
endif

# bookkeeping for configuration summary
loaders = ['stb_image']
net = []
have_winpthread = false
have_python = false
have_wic = false
have_wiccodec = false
have_quicklook_extension = false
quicklook_extension_frameworks = []
quicklook_extension_register_frameworks = []
quicklook_extension_message = 'no'
quicklook_os_supported = true
quicklook_os_reason = 'requires macOS 10.15+'
cpu = host_machine.cpu_family()

has_func_attr_deprecated = false
has_var_attr_deprecated = false

have_unreachable = cc.compiles('''
  static void f(int x) {
    if (x) return;
    __builtin_unreachable();
  }
  int main(void){ f(0); return 0; }
''', name: '__builtin_unreachable()', args: ['-Werror'])
if have_unreachable
  conf.set('HAVE___BUILTIN_UNREACHABLE', 1)
endif

if cc.compiles('''
    __attribute__((deprecated)) void test(void) {}
''')
   has_func_attr_deprecated = true
   conf.set('HAVE_FUNC_ATTRIBUTE_DEPRECATED', 1)
endif

if cc.compiles('''
    __attribute__((deprecated)) int test;
''')
   has_var_attr_deprecated = true
   conf.set('HAVE_VAR_ATTRIBUTE_DEPRECATED', 1)
endif

if cc.compiles('''
    #pragma GCC diagnostic push
    #pragma GCC diagnostic pop
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    __attribute__((deprecated)) void test(void) {}
''')
  conf.set('HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS', 1)
endif

flags = cc.get_supported_arguments([
    '-Wformat=2',
    '-fno-emulated-tls',
    '-Bsymbolic',
])
add_project_arguments(flags, language: 'c')
foreach f: flags
  if f == '-fno-emulated-tls'
    conf.set('HAVE_FNO_EMULATED_TLS', 1)
  elif f == '-Bsymbolic'
    conf.set('HAVE_BSYMBOLIC', 1)
  endif
endforeach

wflags = cc.get_supported_arguments([
    '-Wdeprecated-declarations',
    '-Wunused-but-set-variable',
    '-Wclobbered'
])
foreach f: wflags
  if f == '-Wdeprecated-declarations'
    conf.set('HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS', 1)
  elif f == '-Wunused-but-set-variable'
    conf.set('HAVE_DIAGNOSTIC_UNUSED_BUT_SET_VARIABLE', 1)
  elif f == '-Wclobbered'
    conf.set('HAVE_DIAGNOSTIC_CLOBBERED', 1)
  endif
endforeach

# check common headers
std_headers = [
    'assert.h',
    'math.h',
    'ctype.h',
    'memory.h',
    'unistd.h',
    'stdint.h',
    'sys/unistd.h',
    'getopt.h',
    'sys/types.h',
    'sys/stat.h',
    'fcntl.h',
    'io.h',
    'errno.h',
    'limits.h',
    'sys/time.h',
    'time.h',
    'signal.h',
    'sys/select.h',
    'sys/signal.h',
    'termios.h',
    'sys/ioctl.h',
    'sys/wait.h',
    'spawn.h',
    'inttypes.h',
    'stdio.h',
    'stdlib.h',
    'string.h',
]
foreach h : std_headers
  if cc.has_header(h)
    macro = 'HAVE_' + h.to_upper().replace('.', '_').replace('/', '_')
    conf.set(macro, 1)
  endif
endforeach

# miscellaneous functions
extra_funcs = [
  'assert',
  'malloc',
  'realloc',
  'calloc',
  'memcpy',
  'memmove',
  'memset',
  'getopt_long',
  'memcpy',
  'memmove',
  'setmode',
  '_setmode',
  'signal',
  'strdup',
  'strtoul',
  'calloc',
  'clearerr',
  'stat',
  'setjmp',
  'longjmp',
  'strerror',
  'isatty',
  'strncmp',
  'ldiv',
  'floor',
  'localeconv',
  'pow',
  'select',
  'sqrt',
  'strchr',
  'strerror',
  'strstr',
  'strtol',
  'fork',
  'posix_spawnp'
]
foreach f : extra_funcs
  if cc.has_function(f)
    conf.set('HAVE_' + f.to_upper().replace('.', '_'), 1)
  endif
endforeach

if not cc.has_function('getopt_long') and cc.has_function('getopt')
    conf.set('HAVE_GETOPT', 1)
endif

if host_machine.system() != 'windows'
  if cc.has_function('clock')
    conf.set('HAVE_CLOCK', 1)
  endif
  if cc.has_function('nanosleep')
    conf.set('HAVE_NANOSLEEP', 1)
  endif
endif

# signal handling
foreach s : ['SIGINT', 'SIGTERM', 'SIGHUP']
  if cc.has_header_symbol('signal.h', s)
    conf.set('HAVE_DECL_' + s, 1)
  endif
endforeach

deps = []
reqs = []
link_libs = []

m_dep = cc.find_library('m', required: false)
if m_dep.found()
  link_libs += m_dep
  deps += m_dep
  conf.set('HAVE_LIBM', 1)
endif

png_dep = dependency('libpng', required: get_option('png'))
if png_dep.found()
  deps += png_dep
  reqs += png_dep
  conf.set('HAVE_LIBPNG', 1)
  loaders += 'png'
  conf.set10('HAVE_DECL_PNG_SET_GRAY_1_2_4_TO_8',
             cc.has_function('png_set_gray_1_2_4_to_8',
                             dependencies: png_dep))
  conf.set10('HAVE_DECL_PNG_SET_EXPAND_GRAY_1_2_4_TO_8',
             cc.has_function('png_set_expand_gray_1_2_4_to_8',
                             dependencies: png_dep))
endif

jpeg_dep = dependency('libjpeg', required: get_option('jpeg'))
if jpeg_dep.found()
  deps += jpeg_dep
  reqs += jpeg_dep
  conf.set('HAVE_JPEG', 1)
  loaders += 'jpeg'
endif

curl_dep = dependency('libcurl', required: get_option('curl'))
if curl_dep.found()
  deps += curl_dep
  reqs += curl_dep
  conf.set('HAVE_LIBCURL', 1)
  net += 'libcurl'
endif

gdk_dep = dependency('gdk-pixbuf-2.0', required: get_option('gdk_pixbuf2'))
if gdk_dep.found()
  deps += gdk_dep
  reqs += gdk_dep
  conf.set('HAVE_GDK_PIXBUF2', 1)
  loaders += 'gdk-pixbuf2'
endif

gd_dep = dependency('gd', required: false)
if not gd_dep.found()
  gd_dep = dependency('gdlib', required: get_option('gd'))
endif
if gd_dep.found()
  deps += gd_dep
  reqs += gd_dep
  conf.set('HAVE_GD', 1)
  loaders += 'gd'
  gd_funcs = [
    'gdImageCreateFromGifPtr',
    'gdImageCreateFromPngPtr',
    'gdImageCreateFromBmpPtr',
    'gdImageCreateFromJpegPtrEx',
    'gdImageCreateFromJpegPtr',
    'gdImageCreateFromTgaPtr',
    'gdImageCreateFromWBMPPtr',
    'gdImageCreateFromTiffPtr',
    'gdImageCreateFromGd2Ptr',
    'gdImagePaletteToTrueColor',
    'gdImageCreateFromGifAnimPtr',
  ]
  foreach f : gd_funcs
    conf.set10('HAVE_DECL_' + f.to_upper(),
               cc.has_function(f, prefix: '#include <gd.h>',
                               dependencies: gd_dep))
  endforeach
endif

wic_opt = get_option('wic')
if host_system == 'windows'
  if not wic_opt.disabled()
    have_wic_headers = cc.has_header('windows.h') and cc.has_header('wincodec.h')
    ole32_wic = cc.find_library('ole32', required: false)
    windowscodecs_wic = cc.find_library('windowscodecs', required: false)
    if have_wic_headers and ole32_wic.found() and windowscodecs_wic.found()
      conf.set('HAVE_WIC', 1)
      conf.set('HAVE_WINDOWS_H', 1)
      conf.set('HAVE_WINCODEC_H', 1)
      deps += [ole32_wic, windowscodecs_wic]
      link_libs += [ole32_wic, windowscodecs_wic]
      loaders += 'wic'
      have_wic = true
    elif wic_opt.enabled()
      error('WIC support requested, but required headers or libraries were not found')
    endif
  endif
elif wic_opt.enabled()
  error('WIC support is only available on Windows')
endif

wiccodec_opt = get_option('wiccodec')
wiccodec_message = 'no'
have_wiccodec = false
if host_system == 'windows'
  if not wiccodec_opt.disabled()
    have_wiccodec_headers = cc.has_header('windows.h') and cc.has_header('wincodec.h')
    ole32 = cc.find_library('ole32', required: false)
    windowscodecs = cc.find_library('windowscodecs', required: false)
    advapi32 = cc.find_library('advapi32', required: false)
    user32 = cc.find_library('user32', required: false)
    if have_wiccodec_headers \
         and ole32.found() \
         and windowscodecs.found() \
         and advapi32.found() \
         and user32.found()
      conf.set('HAVE_WICCODEC', 1)
      if not have_wic
        conf.set('HAVE_WINDOWS_H', 1)
        conf.set('HAVE_WINCODEC_H', 1)
      endif
      if cc.has_header('wincodecsdk.h')
        conf.set('HAVE_WINCODECSDK_H', 1)
      endif
      if not have_wic
        deps += [ole32, windowscodecs]
        link_libs += [ole32, windowscodecs]
      endif
      deps += [advapi32, user32]
      link_libs += [advapi32, user32]
      have_wiccodec = true
      wiccodec_message = 'yes'
    elif wiccodec_opt.enabled()
      error('WIC codec support requested, but required headers or libraries were not found')
    endif
  endif
elif wiccodec_opt.enabled()
  error('WIC codec support is only available on Windows')
endif

if get_option('register_dll') and not have_wiccodec
  error('register_dll option requires WIC codec support')
endif

winhttp_opt = get_option('winhttp')
if host_system == 'windows'
  if not winhttp_opt.disabled()
    have_winhttp_headers = cc.has_header('windows.h') and cc.has_header('winhttp.h')
    winhttp_lib = cc.find_library('winhttp', required: false)
    if have_winhttp_headers and winhttp_lib.found()
      conf.set('HAVE_WINHTTP', 1)
      deps += winhttp_lib
      link_libs += winhttp_lib
      net += 'winhttp'
    elif winhttp_opt.enabled()
      error('WinHTTP support requested, but required headers or library were not found')
    endif
  endif
elif winhttp_opt.enabled()
  error('WinHTTP support is only available on Windows')
endif

winpthread_opt = get_option('winpthread')
if host_system == 'windows'
  if winpthread_opt.enabled()
    winpthread_lib = cc.find_library('winpthread', required: true)
    conf.set('WITH_WINPTHREAD', 1)
    conf.set('HAVE_NANOSLEEP', 1)
    conf.set('HAVE_CLOCK', 1)
    deps += winpthread_lib
    link_libs += winpthread_lib
  endif
elif winpthread_opt.enabled()
  error('winpthread is only available on Windows')
endif

cg_opt = get_option('coregraphics')
if host_system == 'darwin'
  if not cg_opt.disabled()
    cg_modules = ['CoreGraphics', 'CoreFoundation', 'ImageIO']
    cg_dep = dependency('appleframeworks', \
                        modules: cg_modules, \
                        required: cg_opt.enabled())
    if cg_dep.found()
      conf.set('HAVE_COREGRAPHICS', 1)
      deps += cg_dep
      link_libs += cg_dep
      loaders += 'coregraphics'
    endif
  endif
elif cg_opt.enabled()
  error('CoreGraphics support requires macOS')
endif

quicklook_preview_opt = get_option('quicklook_preview')
have_quicklook_thumbnailing = false
if host_system == 'darwin'
  if not quicklook_preview_opt.disabled()
    if conf.get('HAVE_COREGRAPHICS', 0) != 1
      if quicklook_preview_opt.enabled()
        error('Quick Look preview requires CoreGraphics support')
      endif
    else
      quicklook_modules = ['QuickLook', 'CoreServices', 'ApplicationServices', 'CoreGraphics']
      quicklook_dep = dependency('appleframeworks',
        modules: quicklook_modules,
        required: quicklook_preview_opt.enabled())
      quicklook_thumb_dep = dependency('appleframeworks',
        modules: ['QuickLookThumbnailing', 'UniformTypeIdentifiers', 'Foundation'],
        required: false)
      if quicklook_dep.found()
        conf.set('HAVE_QUICKLOOK', 1)
        deps += quicklook_dep
        link_libs += quicklook_dep
        loaders += 'quicklook'
        if have_objc and quicklook_thumb_dep.found()
          thumb_test = '''
            #import <Foundation/Foundation.h>
            #import <QuickLookThumbnailing/QuickLookThumbnailing.h>
            int main(void)
            {
                @autoreleasepool {
                    NSURL *url = [NSURL fileURLWithPath:@"/"];
                    CGSize size = CGSizeMake(16.0f, 16.0f);
                    QLThumbnailGenerationRequest *request =
                        [[QLThumbnailGenerationRequest alloc]
                            initWithFileAtURL:url
                                         size:size
                                        scale:1.0f
                          representationTypes:
                              QLThumbnailGenerationRequestRepresentationTypeThumbnail];
                    [[QLThumbnailGenerator sharedGenerator] cancelRequest:request];
                    [request release];
                }
                return 0;
            }
          '''
          if objc.compiles(thumb_test, name: 'quicklook thumbnailing probe',
                           dependencies: quicklook_thumb_dep)
            have_quicklook_thumbnailing = true
            conf.set('HAVE_QUICKLOOK_THUMBNAILING', 1)
            deps += quicklook_thumb_dep
            link_libs += quicklook_thumb_dep
          endif
        endif
      elif quicklook_preview_opt.enabled()
        error('Quick Look preview requested, but required frameworks were not found')
      endif
    endif
  endif
elif quicklook_preview_opt.enabled()
  error('Quick Look preview requires macOS')
endif

quicklook_ext_opt = get_option('quicklook_extension')
if host_system == 'darwin'
  if not quicklook_ext_opt.disabled()
    quicklook_ext_dep = dependency('appleframeworks',
      modules: ['Foundation', \
                'AppKit', \
                'QuickLookUI', \
                'QuickLookThumbnailing', \
                'UniformTypeIdentifiers', \
                'CoreGraphics', \
                'ImageIO'],
      required: quicklook_ext_opt.enabled())
    quicklook_register_dep = dependency('appleframeworks',
      modules: ['Foundation', 'CoreServices', 'UniformTypeIdentifiers'],
      required: quicklook_ext_opt.enabled())
    if quicklook_ext_dep.found() and quicklook_register_dep.found()
      have_quicklook_extension = true
      quicklook_extension_frameworks = [quicklook_ext_dep]
      quicklook_extension_register_frameworks = [quicklook_register_dep]
      quicklook_extension_message = 'yes'
    elif quicklook_ext_opt.enabled()
      error('Quick Look extension requested, but required frameworks were not found')
    else
      quicklook_extension_message = 'no (frameworks unavailable)'
    endif
  endif
elif quicklook_ext_opt.enabled()
  error('Quick Look extension is only available on macOS')
endif

simd_opt = get_option('simd')
if simd_opt.disabled()
  simd_message = 'no'
elif cpu == 'x86_64'
  if cc.has_header('emmintrin.h')
    conf.set('HAVE_EMMINTRIN_H', 1)
    conf.set('HAVE_SSE2', 1)
    simd_message = 'sse2'
  else
    simd_message = 'no'
  endif
elif cpu == 'x86'
  if cc.has_header('emmintrin.h')
    conf.set('HAVE_EMMINTRIN_H', 1)
    if cc.get_id() != 'msvc'
      if cc.has_argument('-msse2') and cc.compiles('''
          #include <emmintrin.h>
          int main(void){
            __m128i a = _mm_setzero_si128(); (void)a; return 0;
          }''', args: ['-msse2'])
        conf.set('HAVE_SSE2', 1)
        simd_message = 'sse2'
        add_project_arguments('-msse2', language: 'c')
      else
        simd_message = 'no'
      endif
    else
      if cc.has_argument('/arch:SSE2') and cc.compiles('''
          #include <emmintrin.h>
          int main(void){
            __m128i a = _mm_setzero_si128(); (void)a; return 0;
          }''', args: ['/arch:SSE2'])
        conf.set('HAVE_SSE2', 1)
        simd_message = 'sse2'
        add_project_arguments('/arch:SSE2', language: 'c')
      else
        simd_message = 'no'
      endif
    endif
  else
    simd_message = 'no'
  endif
elif cpu == 'aarch64'
  if cc.has_header('arm_neon.h')
    conf.set('HAVE_ARM_NEON_H', 1)
    conf.set('HAVE_NEON', 1)
    simd_message = 'neon'
  else
    simd_message = 'no'
  endif
elif cpu.startswith('arm') or cpu == 'aarch32'
  if cc.has_header('arm_neon.h')
    conf.set('HAVE_ARM_NEON_H', 1)
    if cc.get_id() == 'msvc'
      conf.set('HAVE_NEON', 1)
      simd_message = 'neon'
    else
      test_args = ['-mfpu=neon']
      if cc.has_multi_arguments(test_args) and cc.compiles('''
          #include <arm_neon.h>
          int main(void){
            int32x4_t v = vdupq_n_s32(0); (void)v; return 0;
          }''', args: test_args)
        conf.set('HAVE_NEON', 1)
        simd_message = 'neon'
        add_project_arguments(test_args, language: 'c')
      else
        simd_message = 'no 3'
      endif
    endif
  else
    simd_message = 'no 1'
  endif
else
  simd_message = 'no'
endif

config_h = configure_file(output: 'config.h', configuration: conf)
project_inc = include_directories('.', 'include', 'src')

bash_completion_opt = get_option('bashcompletiondir')
bash_completion_dir = ''
if bash_completion_opt != 'disabled'
  if bash_completion_opt == '' or bash_completion_opt == 'auto'
    bash_completion_dir = join_paths(get_option('datadir'), 'bash-completion', 'completions')
  else
    bash_completion_dir = bash_completion_opt
  endif
endif

zsh_completion_opt = get_option('zshcompletiondir')
zsh_completion_dir = ''
if zsh_completion_opt != 'disabled'
  if zsh_completion_opt == '' or zsh_completion_opt == 'auto'
    zsh_completion_dir = join_paths(get_option('datadir'), 'zsh', 'site-functions')
  else
    zsh_completion_dir = zsh_completion_opt
  endif
endif

thumbnailer_message = 'no'
thumbnailer_opt = get_option('thumbnailer_command')
thumbnailer_install = false
sixel2png_opt = get_option('sixel2png')
update_mime_database = find_program('update-mime-database', required : false)

if sixel2png_opt.disabled()
  thumbnailer_message = 'no (requires sixel2png)'
elif thumbnailer_opt.disabled()
  thumbnailer_message = 'no'
else
  datadir_opt = get_option('datadir')
  datadir_path = join_paths(get_option('prefix'), datadir_opt)
  xdg_data_dirs = meson.get_env('XDG_DATA_DIRS')
  if xdg_data_dirs == ''
    xdg_data_dirs = '/usr/local/share:/usr/share'
  endif
  thumbnailer_candidates = []
  foreach xdg_dir : xdg_data_dirs.split(':')
    if xdg_dir != ''
      thumbnailer_candidates += [join_paths(xdg_dir, 'thumbnailers')]
    endif
  endforeach
  home_dir = meson.get_env('HOME')
  if home_dir != ''
    thumbnailer_candidates += [join_paths(home_dir, '.local', 'share', 'thumbnailers')]
  endif
  thumbnailer_candidates += [join_paths(datadir_path, 'thumbnailers')]
  have_thumbnailer_dir = false
  foreach candidate : thumbnailer_candidates
    if fs.exists(candidate)
      have_thumbnailer_dir = true
      break
    endif
  endforeach
  if thumbnailer_opt.enabled()
    thumbnailer_install = true
  elif thumbnailer_opt.auto() and have_thumbnailer_dir
    thumbnailer_install = true
  endif

  if thumbnailer_install
    install_script('tools/sixel-thumbnailer', install_dir: get_option('bindir'))
    install_data('tools/libsixel-thumbnailer.thumbnailer', install_dir: join_paths(get_option('datadir'), 'thumbnailers'))
    install_data('tools/sixel.xml', install_dir: join_paths(get_option('datadir'), 'packages'))
    if update_mime_database.found()
      update_mime_script = join_paths(meson.project_source_root(), 'tools', 'update-mime-database.sh')
      meson.add_install_script(update_mime_script, update_mime_database.path(), join_paths(get_option('prefix'), get_option('datadir')), 'install', skip_if_destdir: true)
    endif
    thumbnailer_message = 'yes'
  elif thumbnailer_opt.auto()
    thumbnailer_message = 'no (thumbnailer dir not found)'
  else
    thumbnailer_message = 'no'
  endif
endif

py_opt = get_option('python')
if py_opt.enabled()
  py_mod = import('python')
  py = py_mod.find_installation(required: py_opt.enabled())
  if py.found()
    have_python = true
    python_message = 'yes: ' + py.get_install_dir()
  elif py_opt.enabled()
    error('Python support requires an interpreter')
  else
    python_message = 'no'
  endif
elif py_opt.enabled()
  error('Python support requires an interpreter')
else
  python_message = 'no'
endif

ruby_message = 'no'
have_ruby = false
# Generate Ruby version file (pure Ruby bindings)
subdir('ruby/lib/libsixel')
ruby_opt = get_option('ruby')
if ruby_opt.enabled()
  ruby = find_program('ruby', required: ruby_opt.enabled())
  if ruby.found()
    have_ruby = true
    # Generate constants from header
    constants_rb = custom_target('ruby_constants',
      input: 'include/sixel.h',
      output: 'constants.rb',
      command: [ruby, 'tools/gen_ruby_constants.rb', '@INPUT@', '@OUTPUT@'],
      build_by_default: true)
    # Install Ruby bindings if requested
    if ruby_opt.enabled()
      sitelibdir = run_command(ruby, '-rrbconfig', '-e', 'print RbConfig::CONFIG["sitelibdir"]').stdout()
      ruby_message = 'yes: ' + sitelibdir
      install_data('ruby/lib/libsixel.rb', install_dir: sitelibdir)
      install_subdir('ruby/lib/libsixel',
        install_dir: join_paths(sitelibdir, 'libsixel'),
        strip_directory: true,
        exclude_files: ['version_runtime.rb'])
      # Install generated files explicitly to override source ones
      install_data(ruby_version_runtime, install_dir: join_paths(sitelibdir, 'libsixel'))
      install_data(constants_rb, install_dir: join_paths(sitelibdir, 'libsixel'))
    endif
  endif
endif

subdir('include')

libsixel_posix_defs = cc.get_supported_arguments([
  '-D_POSIX_SOURCE',
  '-D_POSIX_C_SOURCE=199309L',
])

subdir('src')
subdir('converters')
if have_wiccodec
  subdir('wic')
endif


if have_quicklook_extension
  subdir('quicklook-extension')
endif
if have_python
  subdir('python')
endif


if get_option('tests')
  if have_ruby
    env = environment()
    bindir = meson.current_build_dir()
    src_build_dir = join_paths(bindir, 'src')
    env.set('LD_LIBRARY_PATH', src_build_dir + ':' + env.get('LD_LIBRARY_PATH', ''))
    env.set('DYLD_LIBRARY_PATH', src_build_dir + ':' + env.get('DYLD_LIBRARY_PATH', ''))
    ruby_build_libdir = join_paths(bindir, 'ruby', 'lib')
    test('ruby-bindings', ruby,
         args: ['-I', ruby_build_libdir, '-I', 'ruby/lib', '-I', 'ruby/test', 'ruby/test/test_libsixel.rb'],
         env: env)
  endif
endif

pkgconfig_dir_rel = join_paths(get_option('libdir'), 'pkgconfig')
pkgconfig_dir = join_paths(get_option('prefix'), pkgconfig_dir_rel)

req_names = []
foreach r : reqs
  req_names += r.name()
endforeach

pc_conf = configuration_data()
pc_conf.set('prefix', get_option('prefix'))
pc_conf.set('exec_prefix', '${prefix}')
pc_conf.set('libdir', join_paths('${exec_prefix}', get_option('libdir')))
pc_conf.set('includedir', join_paths('${prefix}', get_option('includedir')))
pc_conf.set('PACKAGE_DESCRIPTION', 'A lightweight, fast implementation of DEC SIXEL graphics codec')
pc_conf.set('PACKAGE_VERSION', meson.project_version())
pc_conf.set('REQUIRES_PRIVATE', ' '.join(req_names))
pc_conf.set('LIBS_PRIVATE', '')  # TODO: how to get the correct linker flags for static linking in meson?

configure_file(
  input: 'libsixel.pc.in',
  output: 'libsixel.pc',
  configuration: pc_conf,
  install: true,
  install_dir: pkgconfig_dir_rel)

net_summary = ' '.join(net)
loaders_summary = ' '.join(loaders)

bash_completion_message = 'disabled'
if bash_completion_dir != ''
  bash_completion_message = bash_completion_dir
endif

zsh_completion_message = 'disabled'
if zsh_completion_dir != ''
  zsh_completion_message = zsh_completion_dir
endif

message('')
message('libsixel was configured as follows')
message('')
message('       Loader component    : ' + loaders_summary)
message('       Network access      : ' + net_summary)
message('       libwinpthread       : ' + (have_winpthread ? 'yes' : 'no'))
message('       pkg-config dir      : ' + pkgconfig_dir)
message('       Bash completion dir : ' + bash_completion_message)
message('       Zsh completion dir  : ' + zsh_completion_message)
message('       QuickLook extension : ' + quicklook_extension_message)
message('       WIC codec           : ' + wiccodec_message)
message('       python bindings     : ' + python_message)
message('       ruby bindings       : ' + ruby_message)
message('       Thumbnailer command : ' + thumbnailer_message)
message('       SIMD                : ' + simd_message)
message('       gcov integration    : ' + (get_option('gcov') ? 'yes' : 'no'))
message('       debugging           : ' + (get_option('debug') ? 'yes' : 'no'))
message('       tests               : ' + (get_option('tests') ? 'yes' : 'no'))
message('')
# vim: set et ts=2:
