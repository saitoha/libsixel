lsqa_inc = [project_inc]
lsqa_deps = [libsixel_dep]
if m_dep.found()
  lsqa_deps += m_dep
endif
lsqa_args = libsixel_posix_defs

src_build_dir = join_paths(meson.project_build_root(), 'src')
lsqa_sources = ['lsqa.c']
lsqa_dependencies = lsqa_deps
lsqa_c_args = lsqa_args
lsqa_objc_args = lsqa_c_args
lsqa_rpath = src_build_dir

if amalgamated_lib_enabled and amalgamated_tools_enabled
  lsqa_sources = [amalgamation_out]
  lsqa_dependencies = deps
  lsqa_c_args = libsixel_posix_defs + ['-DBUILD_LSQA']
  lsqa_objc_args = lsqa_c_args
  lsqa_rpath = ''
endif

lsqa_bin = executable('lsqa',
           lsqa_sources,
           include_directories: lsqa_inc,
           build_rpath: lsqa_rpath,
           dependencies: lsqa_dependencies,
            c_args: lsqa_c_args,
            install: true)
if shebang_enabled
  shebang_targets += [custom_target('lsqa_shebang',
    input: lsqa_bin,
    output: 'lsqa.shebang.stamp',
    command: [shebang_helper, shebang_file, '@INPUT@'],
    depend_files: shebang_file,
    build_by_default: true)]
endif
set_variable('lsqa_bin', lsqa_bin)
