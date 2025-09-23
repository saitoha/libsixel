# Ruby Examples for libsixel

These are pure-Ruby examples using the Fiddle (FFI) bindings provided in `ruby/lib/`.

Requirements
- Ruby >= 2.6
- libsixel shared library available in your runtime library path
  - Linux: set `LD_LIBRARY_PATH` or install libsixel to a standard lib path
  - macOS: set `DYLD_LIBRARY_PATH` or install via a package manager

Build + Install (optional)
- Autotools
  - `./configure --enable-ruby && make && make install`
- Meson
  - `meson setup build -D ruby=enabled && ninja -C build && ninja -C build install`

Run
1) Encode a file to SIXEL and write to STDOUT
```
$ ruby examples/ruby/encoder_stdout.rb images/egret.jpg
```

2) Generate a synthetic RGB gradient and output as SIXEL to STDOUT
```
$ ruby examples/ruby/output_gradient.rb
```

If the libsixel shared library is not in a standard path, set the environment before running:
```
$ export LD_LIBRARY_PATH=/path/to/libsixel/src/.libs:$LD_LIBRARY_PATH     # Linux
$ export DYLD_LIBRARY_PATH=/path/to/libsixel/src/.libs:$DYLD_LIBRARY_PATH # macOS
```

Notes
- The examples append `ruby/lib` to `$LOAD_PATH` to use the local bindings without installation.
- See `ruby/README.md` for more usage details and available APIs.

License
- CC0 - "No Rights Reserved"

