# Embed the Vulkan programs of the "vulkan.lookup-rgb8.v1" ABI at build
# time; nothing is loaded from disk. Every GLSL chunk is embedded as text:
# the runtime composes consumer prefix, lookup policy and consumer body,
# and that exact text identifies the program. When glslc is available the
# whole (consumer x policy) matrix and every policy builder is compiled to
# SPIR-V here; libshaderc compiles any other composition at runtime.
vulkan_headers = []
vulkan_consumers = [
  ['../gpu-palette-fused-prefix.glsl', '../gpu-palette-fused-suffix.glsl'],
  ['../dither-policy-lso2-gpu-prefix.glsl', '../dither-policy-lso2-gpu.glsl'],
  ['../dither-policy-fs-gpu-prefix.glsl', '../dither-policy-fs-gpu.glsl'],
  ['../dither-policy-atkinson-gpu-prefix.glsl',
   '../dither-policy-atkinson-gpu.glsl'],
  ['../dither-policy-jajuni-gpu-prefix.glsl',
   '../dither-policy-jajuni-gpu.glsl'],
  ['../dither-policy-stucki-gpu-prefix.glsl',
   '../dither-policy-stucki-gpu.glsl'],
  ['../dither-policy-burkes-gpu-prefix.glsl',
   '../dither-policy-burkes-gpu.glsl'],
  ['../dither-policy-sierra1-gpu-prefix.glsl',
   '../dither-policy-sierra1-gpu.glsl'],
  ['../dither-policy-sierra2-gpu-prefix.glsl',
   '../dither-policy-sierra2-gpu.glsl'],
  ['../dither-policy-sierra3-gpu-prefix.glsl',
   '../dither-policy-sierra3-gpu.glsl'],
]
vulkan_builder_prefix = '../gpu-lookup-builder-prefix.glsl'
vulkan_policies = [
  '../lookup-policy-none-gpu.glsl',
  '../lookup-policy-fhedt-gpu.glsl',
  '../lookup-policy-bitlut-gpu.glsl',
  '../lookup-policy-eytzinger-gpu.glsl',
  '../lookup-policy-ordered-gpu.glsl',
  '../lookup-policy-vptree-gpu.glsl',
  '../lookup-policy-rbc-gpu.glsl',
  '../lookup-policy-mahalanobis-gpu.glsl',
]
if have_vulkan
  vulkan_chunks = [vulkan_builder_prefix] + vulkan_policies
  foreach consumer : vulkan_consumers
    vulkan_chunks += consumer
  endforeach
  foreach input : vulkan_chunks
    name = fs.stem(input)
    vulkan_headers += custom_target(name + '-vulkan-shader',
      input: input,
      output: name + '-shader.inc.h',
      command: [sh, metal_embed, '@INPUT@', '@OUTPUT@'],
      depend_files: metal_encoder,
      build_by_default: true)
  endforeach
  if have_vulkan_prebuilt_spirv
    vulkan_matrix_args = []
    foreach consumer : vulkan_consumers
      vulkan_matrix_args += (meson.current_source_dir() / consumer[0] + ':'
        + meson.current_source_dir() / consumer[1])
    endforeach
    vulkan_headers += custom_target('lookup-programs-spirv',
      input: vulkan_chunks,
      output: 'lookup-programs-spirv.inc.h',
      command: [sh, files('../../tools/gen-vulkan-programs.sh'), glslc,
                '@OUTPUT@',
                meson.current_source_dir() / vulkan_builder_prefix]
               + vulkan_matrix_args + ['--']
               + files(vulkan_policies),
      build_by_default: true)
  endif
endif
