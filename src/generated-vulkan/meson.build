# Embed Vulkan compute shaders at build time; nothing is loaded from disk.
# GLSL text is embedded when libshaderc can compile it at runtime; SPIR-V is
# embedded when glslc is available at build time. At least one form exists
# whenever have_vulkan is true.
vulkan_headers = []
vulkan_sources = files(
  '../gpu-palette-vulkan.comp',
  '../lookup-policy-fhedt-vulkan.comp',
  '../lookup-policy-bitlut-vulkan.comp',
  '../dither-diffuse-vulkan.comp',
)
if have_vulkan
  foreach input : vulkan_sources
    name = fs.stem(input)
    if have_shaderc
      vulkan_headers += custom_target(name + '-vulkan-shader',
        input: input,
        output: name + '-shader.inc.h',
        command: [sh, metal_embed, '@INPUT@', '@OUTPUT@'],
        depend_files: metal_encoder,
        build_by_default: true)
    endif
    if have_vulkan_prebuilt_spirv
      spirv = custom_target(name + '-spv',
        input: input,
        output: name + '.spv',
        command: [glslc, '-O', '--target-env=vulkan1.2',
                  '@INPUT@', '-o', '@OUTPUT@'],
        build_by_default: true)
      vulkan_headers += custom_target(name + '-vulkan-spirv',
        input: spirv,
        output: name + '-spirv.inc.h',
        command: [sh, files('../../tools/embed-spirv.sh'),
                  '@INPUT@', '@OUTPUT@'],
        build_by_default: true)
    endif
  endforeach
endif
