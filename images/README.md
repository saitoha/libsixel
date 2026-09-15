# Sample images

The `snake*` samples derive from Jon Sullivan's public-domain snake photograph; see [LICENSE](../LICENSE) for attribution. `snake.png` is the source for the DDS, JPEG XR (HDP), HEIC, AVIF, and JPEG XL samples below. They retain its 600×450 RGB raster. The existing `snake.webp` supplies WebP coverage.

## Regenerating format samples

Run these commands from the repository root. They use optional image-conversion tools only; normal builds and tests consume the checked-in samples. Use lossless RGB encoding so decoded pixels can be compared exactly with `snake.png`.

```sh
magick images/snake.png -strip -alpha off \
  -define dds:compression=none -define dds:mipmaps=0 images/snake.dds
cjxl images/snake.png images/snake.jxl --distance=0 --effort=7
heif-enc --lossless --no-alpha --matrix_coefficients=0 \
  --colour_primaries=1 --transfer_characteristic=13 \
  -o images/snake.heic images/snake.png
heif-enc --avif --lossless --no-alpha --matrix_coefficients=0 \
  --colour_primaries=1 --transfer_characteristic=13 \
  -o images/snake.avif images/snake.png
```

JPEG XR generation uses Pillow, NumPy, and imagecodecs in an optional Python environment:

```python
from pathlib import Path

import imagecodecs
import numpy as np
from PIL import Image

pixels = np.array(Image.open("images/snake.png").convert("RGB"))
encoded = imagecodecs.jpegxr_encode(pixels, level=1.0)
assert np.array_equal(imagecodecs.jpegxr_decode(encoded), pixels)
Path("images/snake.hdp").write_bytes(encoded)
```

The replacement samples were generated with ImageMagick 7.1.2-25, libjxl 0.11.2, libheif 1.23.0, and imagecodecs 2026.8.16 (jxrlib 1.1). Encoder versions can change the compressed bytes; the decoded RGB raster is the invariant.
