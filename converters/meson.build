if get_option('png').enabled()
  has_png = 'true'
else
  has_png = ''
endif
if get_option('libcurl').enabled()
  has_libcurl = 'true'
else
  has_libcurl = ''
endif

bash_for_tests = find_program('bash', required: get_option('tests').enabled())

test_env = environment()
test_env.set('BUILDDIR', meson.current_build_dir())
test_env.set('TOP_SRCDIR', meson.source_root())

if get_option('img2sixel').enabled()
  e = executable('img2sixel', 'img2sixel.c', include_directories: [inc, inc_config], link_with: libsixel)
  install_man('img2sixel.1')

  if get_option('tests').enabled()
    warning('Note: tests currently require the Bourne-Again Shell (bash)')
    test('img2sixel tests', bash_for_tests, args: ['./img2sixel_tests.sh', has_png, has_libcurl, 'silent'], is_parallel: false, workdir: '/'.join([meson.source_root(), 'converters']), priority: 1, timeout: 120, env: test_env)
  endif
endif

if get_option('sixel2png').enabled()
  executable('sixel2png', 'sixel2png.c', include_directories: [inc, inc_config], link_with: libsixel)
  install_man('sixel2png.1')

  if get_option('tests').enabled()
    test('sixel2png tests', bash_for_tests, args: ['./sixel2png_tests.sh', has_png, has_libcurl, 'silent'], is_parallel: false, workdir: '/'.join([meson.source_root(), 'converters']), priority: 0, env: test_env)
  endif
endif

install_data('shell-completion/bash/img2sixel', install_dir: bashcompletiondir)
install_data('shell-completion/zsh/_img2sixel', install_dir: zshcompletiondir)
