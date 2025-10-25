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

# Kornel's undither presets mirror the CLI table so you can toggle the
# refine pass:
#
#   +-------------+----------------------------------------+
#   | k_undither  | Kornel's undither without refinement   |
#   | k_undither+ | Kornel's undither with post-refine run |
#   +-------------+----------------------------------------+
#
decoder.setopt 'd', 'k_undither'
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
