# libsixel-wheel

`libsixel-wheel` packages the `libsixel` shared library inside a Python wheel
so that users can rely on a pure `ctypes` interface without installing system
packages. The vendored binary is preloaded when the package is imported, and
`ctypes.CDLL` is used to expose the native API.

```python
import libsixel_wheel as sixel

sixel.load()
print("ready:", sixel.is_ready())
print("using:", sixel.lib_path())
```

The wheel is self-contained. Environment variables such as `LD_LIBRARY_PATH`,
`DYLD_LIBRARY_PATH`, or `PATH` are not required.
