# Libsixel (Ruby)

Pure Ruby bindings for libsixel using Fiddle (Ruby's built-in FFI), covering the public C API. No native extension build is required; the gem loads the libsixel shared library at runtime.

## Requirements

- Ruby >= 2.6
- libsixel shared library available at runtime (e.g. installed via your OS package manager)

On macOS, ensure `DYLD_LIBRARY_PATH` or system paths include `libsixel`. On Linux, ensure `LD_LIBRARY_PATH` or system library directories include it.

## Installation

Add to your Gemfile:

```ruby
gem 'libsixel-ruby'
```

Or install directly:

```
$ gem install libsixel-ruby
```

With libsixel build systems:

- Autotools
  - `./configure --enable-ruby && make && make install`
- Meson
  - `meson setup build -D ruby=enabled && ninja -C build && ninja -C build install`

## Usage

High-level encoder/decoder (compatible with the previous API):

```ruby
require 'libsixel'

encoder = Encoder.new
encoder.setopt 'p', 16      # colors
encoder.setopt 'w', '200'   # width
encoder.encode 'images/egret.jpg'

decoder = Decoder.new
decoder.decode              # honors options set with setopt

# Kornel's undither preset mirrors the CLI table:
#
#   +-------------+----------------------------------------+
#   | k_undither  | Kornel's undither filter               |
#   +-------------+----------------------------------------+
#
decoder.setopt 'd', 'k_undither'
```

## Float32 precision pipeline

The Ruby bindings can participate in the same float32 workflow that the
CLI exposes.  This is useful when you want to keep HDR or wide-gamut
sources in a float format until the final palette is emitted.

1. Ensure libsixel itself was built with the Ruby bindings enabled so
   the gem can load the new dithering entry points.
2. Opt into the float pipeline by setting ``ENV["SIXEL_FLOAT32_DITHER"]
   = "1"`` before instantiating an ``Encoder``.  This mirrors
   ``img2sixel -.float32``.
3. Request the float precision flag through ``encoder.setopt('.',
   'float32')`` so that automatic image loading uses the expanded path.
4. Select a working colorspace via ``encoder.setopt('W', 'linear')`` or
   ``'oklab'`` if you want libsixel to convert the source prior to
   quantization without dropping back to 8-bit.
5. When passing custom buffers to ``encode_bytes`` choose one of the
   float pixel format constants exposed via ``Libsixel::API``:

   | Constant name                                      | Use case              |
   | -------------------------------------------------- | --------------------- |
   | ``SIXEL_PIXELFORMAT_RGBFLOAT32``                   | Gamma-encoded RGB     |
   | ``SIXEL_PIXELFORMAT_LINEARRGBFLOAT32``             | Linear-light RGB data |
   | ``SIXEL_PIXELFORMAT_OKLABFLOAT32``                 | OKLab intermediates   |

   Supplying these constants prevents the implicit byte conversion
   performed by legacy releases.

```ruby
require 'libsixel'

ENV['SIXEL_FLOAT32_DITHER'] = '1'

encoder = Encoder.new
encoder.setopt '.', 'float32'
encoder.setopt 'W', 'oklab'
encoder.setopt 'p', '256'
encoder.encode 'images/snake.exr'
```

Low-level C API (ctypes-style) via `Libsixel::API`:

```ruby
require 'libsixel'

# Compute depth for a pixel format value (e.g. SIXEL_PIXELFORMAT_RGB888)
depth = Libsixel::API.sixel_helper_compute_depth(0x1003)

# Custom output using sixel_output_new
cb = Fiddle::Closure::BlockCaller.new(Fiddle::TYPE_INT,
  [Fiddle::TYPE_VOIDP, Fiddle::TYPE_INT, Fiddle::TYPE_VOIDP]
) do |data, size, _priv|
  $stdout.write(Fiddle::Pointer.new(data)[0, size])
  size
end

outpp = Libsixel::API::Util.make_outptr
status = Libsixel::API.sixel_output_new(outpp, cb, 0, 0)
raise Libsixel::API::Err.message(status) if Libsixel::API.failed?(status)
output = Libsixel::API::Util.read_outptr(outpp)
```

## Shared Library Loading

The gem tries these names: `sixel`, `libsixel`, `sixel-1`, `libsixel-1`, `msys-sixel`, `cygsixel`, `libsixel.dylib`. If loading fails, set your runtime library path or install libsixel.

## Testing

From the repo root:

```
$ rake test
# or
$ ruby -I ruby/lib -I ruby/test ruby/test/test_libsixel.rb
```

With libsixel build systems:

- Autotools: `make check`（`--enable-ruby` 有効時に Ruby テストも実行）
- Meson: `meson test -C build`（`-D ruby=enabled` 有効時に Ruby テストも実行）

Tests that rely on the shared library are skipped automatically if it is not found. The version test always runs.

## Contributing

1. Fork it
2. Create your feature branch (`git checkout -b my-new-feature`)
3. Commit your changes (`git commit -m 'Add some feature'`)
4. Push to the branch (`git push origin my-new-feature`)
5. Create a new Pull Request
