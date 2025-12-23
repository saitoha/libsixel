fs = import('fs')

# Collect all sample images so `meson dist` ships the full corpus.
pngsuite_find = 'cd @0@ && find pngsuite -type f -name "*.png" | sort'
pngsuite_cmd = pngsuite_find.format(meson.current_source_dir())
# Meson 0.59 does not provide fs.glob(), so rely on find(1) for recursion.
pngsuite_result = run_command('sh', '-c', pngsuite_cmd, check: true)
pngsuite_output = pngsuite_result.stdout().strip()
pngsuite_list = pngsuite_output.split('\n')
pngsuite_images = []
if pngsuite_list != ['']
  pngsuite_images = files(pngsuite_list)
endif
root_images = files('autumn.dds',
  'autumn.hdp',
  'autumn.heic',
  'autumn.png',
  'autumn.webp',
  'egret.jpg',
  'fisheye.dds',
  'fisheye.hdp',
  'fisheye.heic',
  'fisheye.png',
  'fisheye.webp',
  'libsixel.avif',
  'libsixel.jxl',
  'libsixel.webp',
  'map16-palette.png',
  'map16.png',
  'map64.six',
  'map8-palette.png',
  'map8.png',
  'map8.six',
  'seq2gif.gif',
  'snake-ascii.pbm',
  'snake-ascii.pgm',
  'snake-ascii.ppm',
  'snake-grayscale.jpg',
  'snake-grayscale.png',
  'snake-monochrome.png',
  'snake-palette.png',
  'snake-progressive.jpg',
  'snake.bmp',
  'snake.gif',
  'snake.jpg',
  'snake.pbm',
  'snake.pgm',
  'snake.png',
  'snake.ppm',
  'snake.six',
  'snake.tga',
  'snake.tiff',
  'vimperator3.png')

meson.add_dist_script('sh', '-c', 'true', pngsuite_images + root_images)
