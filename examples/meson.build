# Ensure example scripts under `examples` are part of `meson dist`.
windows_samples = files('windows/wsh_decoder_sample.vbs')

# Pass the Windows sample to the dist script as a positional argument to keep
# Meson's Meson script interface happy while ensuring the file is packaged.
meson.add_dist_script('sh', '-c', 'true', windows_samples)
