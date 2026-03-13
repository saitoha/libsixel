# Libsixel (PHP)

Pure PHP bindings for libsixel using PHP FFI (`ext-ffi`).

The bindings load `libsixel` at runtime and do not require building a PHP C extension.

## Requirements

- PHP >= 7.4
- `ext-ffi` enabled
- `libsixel` shared library (`.so`, `.dylib`, or `.dll`)

The loader checks in this order:

1. `php/src/_libs` (bundled package artifacts)
2. `LIBSIXEL_LIBPATH` (exact library file)
3. `LIBSIXEL_LIBDIR` (directory scan)
4. system loader names (`sixel`, `libsixel`, ...)

## Usage

```php
<?php
require __DIR__ . '/src/autoload.php';

use Libsixel\Constants;
use Libsixel\Encoder;

$encoder = new Encoder();
$encoder->setopt(Constants::SIXEL_OPTFLAG_COLORS, "16");
$encoder->setopt(Constants::SIXEL_OPTFLAG_WIDTH, "200");
$encoder->encode("images/egret.jpg");
$encoder->close();
```

Backward-compatible global aliases are also provided:

- `SixelEncoder`
- `SixelDecoder`

## Build bundled package artifact

Autotools:

```sh
./configure --enable-php
make
ls php/dist/libsixel-php-*
```

Meson:

```sh
meson setup build -D php=enabled
meson compile -C build
ls build/php/dist/libsixel-php-*
```

Standalone builder:

```sh
php php/package_builder.php \
  --libdir build/src/.libs \
  --distdir php/dist \
  --version 1.0.0
```

## Notes

- `ffi.enable=preload` environments may restrict where FFI can be used.
- The package archive is platform-specific because it bundles a native shared library.
