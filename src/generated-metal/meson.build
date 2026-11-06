# Embed source at build time; no shader files are loaded at runtime.
metal_headers = []
metal_sources = files(
  '../retained-plane-gpu.metal',
  '../dither-policy-phased-gpu.metal',
  '../lookup-policy-ordered-gpu.metal',
  '../lookup-policy-none-fractional-gpu.metal',
  '../lookup-policy-eytzinger-fractional-gpu.metal',
  '../lookup-policy-gridlut-fractional-gpu.metal',
  '../lookup-policy-blockcull-fractional-gpu.metal',
  '../lookup-policy-bitlut-fractional-gpu.metal',
  '../lookup-policy-fhedt-fractional-gpu.metal',
  '../lookup-policy-delaunay-fractional-gpu.metal',
  '../lookup-policy-vptree-gpu.metal',
  '../lookup-policy-rbc-gpu.metal',
  '../lookup-policy-mahalanobis-gpu.metal',
  '../lookup-policy-certlut-gpu.metal',
  '../gpu-palette-legacy-types.metal',
  '../gpu-palette-legacy-query.metal',
  '../lookup-policy-gridlut-gpu.metal',
  '../lookup-policy-delaunay-gpu.metal',
  '../lookup-policy-eytzinger-gpu.metal',
  '../lookup-policy-fhedt-gpu-query.metal',
  '../gpu-palette-legacy-pixel.metal',
  '../gpu-palette-legacy-apply.metal',
  '../gpu-dequant-fast4.metal',
  '../lookup-policy-none-gpu.metal',
  '../lookup-policy-blockcull-gpu.metal',
  '../lookup-policy-fhedt-gpu.metal',
  '../lookup-policy-bitlut-gpu.metal',
  '../gpu-palette-fused-prefix.metal',
  '../gpu-palette-fused-suffix.metal',
)
metal_embed = files('../../tools/embed-metal.sh')
metal_encoder = files('../../tools/embed-metal.awk')
foreach input : metal_sources
  name = fs.stem(input)
  metal_headers += custom_target(name + '-shader',
    input: input,
    output: name + '-shader.inc.h',
    command: [sh, metal_embed, '@INPUT@', '@OUTPUT@'],
    depend_files: metal_encoder,
    build_by_default: true)
endforeach
