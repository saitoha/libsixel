lsqa_inc = [project_inc]
lsqa_deps = [libsixel_dep]
if m_dep.found()
  lsqa_deps += m_dep
endif
lsqa_args = libsixel_posix_defs
ort_dep = disabler()
model_root_opt = get_option('modeldir')
if model_root_opt == ''
  model_root = join_paths(get_option('datadir'),
                          meson.project_name(),
                          'models')
else
  model_root = model_root_opt
endif
lpips_model_root_opt = get_option('lpips_model_dir')
if lpips_model_root_opt == ''
  lpips_model_root = model_root
else
  lpips_model_root = lpips_model_root_opt
endif
lpips_model_dir = join_paths(lpips_model_root, 'lpips')
lsqa_args += ['-DSIXEL_LPIPS_MODEL_DIR="' + lpips_model_root + '"']

onnxruntime_message_local = 'no'
lpips_opt = get_option('onnxruntime')
if not lpips_opt.disabled()
  ort_inc_dirs = []
  ort_dep = dependency('libonnxruntime', required: false)
  ort_lib_dir_opt = get_option('onnxruntime_lib_dir')
  if not ort_dep.found()
    ort_lib = cc.find_library('onnxruntime',
                              dirs: ort_lib_dir_opt == '' ? [] : [ort_lib_dir_opt],
                              required: false)
    if ort_lib.found()
      ort_dep = declare_dependency(link_with: ort_lib)
    endif
  endif
  ort_inc_opt = get_option('onnxruntime_include_dir')
  if ort_inc_opt != ''
    ort_inc_dirs += include_directories(ort_inc_opt, is_system: true)
  endif
  ort_header_found = cc.has_header('onnxruntime_c_api.h',
                                   include_directories: ort_inc_dirs)
  if ort_dep.found() and ort_header_found
    conf.set('HAVE_ONNXRUNTIME', 1)
    onnxruntime_message_local = 'yes'
    lsqa_inc += ort_inc_dirs
    lsqa_deps += ort_dep
    lpips_inc = lsqa_inc
    lpips_deps = lsqa_deps
    lpips_args = lsqa_args
    lpips_exe = executable('lpips',
               '../tools/lpips.c',
               include_directories: lpips_inc,
               dependencies: lpips_deps,
               c_args: lpips_args,
               install: false)
    if shebang_enabled
      shebang_targets += [custom_target('lpips_shebang',
        input: lpips_exe,
        output: 'lpips.shebang.stamp',
        command: [shebang_helper, shebang_file, '@INPUT@'],
        depend_files: shebang_file,
        build_by_default: true)]
    endif
    install_data(['../models/lpips/lpips_diff.onnx',
                  '../models/lpips/lpips_feature.onnx'],
                 install_dir: lpips_model_dir)
  else
    if lpips_opt.enabled()
      error('ONNX Runtime headers or library not found, cannot build lpips helper')
    else
      message('Skipping lpips helper: ONNX Runtime not detected')
    endif
  endif
endif
set_variable('onnxruntime_message', onnxruntime_message_local)

src_build_dir = join_paths(meson.project_build_root(), 'src')
lsqa_sources = ['lsqa.c']
lsqa_dependencies = lsqa_deps
lsqa_c_args = lsqa_args
lsqa_objc_args = lsqa_c_args
lsqa_rpath = src_build_dir

if amalgamated_lib_enabled and amalgamated_tools_enabled
  lsqa_sources = [amalgamation_out]
  lsqa_dependencies = deps
  if ort_dep.found()
    lsqa_dependencies += ort_dep
  endif
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
