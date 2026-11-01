fs = import('fs')

# Collect all sample images so `meson dist` ships the full corpus.
# The corpus has one fixed category level; globbing needs no external enumerator.
pngsuite_result = run_command('sh', '-c',
  'cd "$1" && printf "%s\\n" pngsuite/*/*.png',
  'pngsuite-list', meson.current_source_dir(), check: true)
pngsuite_output = pngsuite_result.stdout().strip()
pngsuite_list = pngsuite_output.split('\n')
pngsuite_images = []
if pngsuite_list != ['']
  pngsuite_images = files(pngsuite_list)
endif
root_images = files('README.md',
  'egret.jpg',
  'map16-palette.png',
  'map16.png',
  'map64.six',
  'map8-palette.png',
  'map8.png',
  'map8.six',
  'snake-ascii.pbm',
  'snake-ascii.pgm',
  'snake-ascii.ppm',
  'snake-grayscale.jpg',
  'snake-grayscale.png',
  'snake-monochrome.png',
  'snake-palette.png',
  'snake-progressive-16x16.jpg',
  'snake-progressive.jpg',
  'snake.avif',
  'snake.bmp',
  'snake.dds',
  'snake.gif',
  'snake.hdp',
  'snake.heic',
  'snake.jpg',
  'snake.jxl',
  'snake.pbm',
  'snake.pgm',
  'snake.png',
  'snake.webp',
  'snake.ppm',
  'snake.six',
  'snake.tga',
  'snake.tiff',
  'vimperator3.png')

meson.add_dist_script('sh', '-c', 'true', pngsuite_images + root_images)
