#
# @configure_input@
#
SUBDIRS = include . src converters assessment tools python examples images
if WANT_AMALGAMATED_SOURCES
SUBDIRS += amalgamation
endif
if ENABLE_GDK_PIXBUF_LOADER
SUBDIRS += gdk-pixbuf-loader
endif
if HAVE_RUBY
SUBDIRS += ruby
endif
if HAVE_PHP
SUBDIRS += php
endif
if HAVE_WICCODEC
SUBDIRS += wic
endif
if HAVE_APPLE_QUICKLOOK_EXTENSION
SUBDIRS += quicklook-extension
endif
if COND_FUZZ
SUBDIRS += fuzz
endif
SUBDIRS += tests

DIST_SUBDIRS = \
        include \
        src \
        converters \
        assessment \
        tools \
        python \
        examples \
        images \
        fuzz \
        ruby \
        php \
        wic \
        gdk-pixbuf-loader \
        quicklook-extension \
        amalgamation \
        tests

# Keep depcomp in the aux directory so "make dist" can stage it correctly.
EXTRA_DIST = LICENSE build.md build-aux/depcomp \
    build-aux/dependency-bootstrap.am \
    tools/generate_cms_quality_references.py \
    tools/generate_color_fixtures.py \
    tools/generate_palette_space_fixtures.py \
    tools/generate_cover_anchor_fixtures.py \
    tools/generate_clamp_band_fixtures.py \
    tools/generate_snap_native_fixtures.py \
    tools/generate_destination_snap_fixtures.py \
    tools/generate_p3_snap_fixtures.py \
    tools/generate_output_native_fixtures.py \
    tools/generate_builtin_native_fixtures.py \
	mkdocs.yml \
	m4/ax_count_cpus.m4 \
	package.json.in.in \
	build-aux/lso-tap-driver.sh.in \
	build-aux/read-check-test-list.sh \
	build-aux/cli-color-tty-probe.c \
	build-aux/resolve-binding-test-common.sh \
	build-aux/resolve-config-h-exports.sh \
	build-aux/sanitizer-ignorelist.txt \
	build-aux/resolve-python-test-venv.sh \
	build-aux/resolve-ruby-test-venv.sh \
	build-aux/resolve-perl-test-venv.sh \
	build-aux/resolve-php-test-venv.sh \
	build-aux/install-emscripten-sidecar.sh \
	build-aux/meson-test-with-failures.sh \
	build-aux/resolve-test-tool-paths.sh.in \
	docs/AGENTS.md \
	docs/README.md \
	docs/build/README.md \
	docs/build/autotools.md \
	docs/build/configure-macros.md \
	docs/build/meson.md \
	docs/build/meson-history.md \
	docs/build/support-scripts.md \
	docs/build/test-harness.md \
	docs/assets/javascripts/mathjax.js \
	docs/platform-support.md \
	docs/project-history.md \
	docs/sixel-format/README.md \
	docs/sixel-format/patents-and-royalties.md \
	docs/misc/algorithm-patents.md \
	docs/sixel-format/specification-layers.md \
	docs/sixel-format/terminal-graphics-history/README.md \
	docs/sixel-format/terminal-graphics-history/figures/terminal-graphics-timeline-mobile.svg \
	docs/sixel-format/terminal-graphics-history/figures/terminal-graphics-timeline-wide.svg \
	docs/sixel-format/terminal-graphics-history/figures/terminal-graphics-timeline.json \
	docs/protocols/graphics-color-management.md \
	docs/encoder/colorspace-handling/README.md \
	docs/encoder/colorspace-handling/figures/encoding-pipeline-mobile.svg \
	docs/encoder/colorspace-handling/figures/encoding-pipeline-wide.svg \
	docs/encoder/colorspace-handling/figures/resize-boundaries-mobile.svg \
	docs/encoder/colorspace-handling/figures/resize-boundaries-wide.svg \
	docs/encoder/output/high-color/README.md \
	docs/encoder/output/high-color/measurement.md \
	docs/encoder/output/high-color/measurements/high-color-comparison.csv \
	docs/encoder/output/high-color/measurements/high-color-results.png \
	docs/encoder/output/high-color/measurements/high-color-run.json \
	docs/encoder/output/high-color/measurements/high-color-speed.csv \
	docs/encoder/output/high-color/measurements/high-color-visual-comparison.png \
	docs/encoder/output/high-color/study/color-occupancy.png \
	docs/encoder/output/high-color/study/gradient-256-fs.png \
	docs/encoder/output/high-color/study/gradient-256-fs.six \
	docs/encoder/output/high-color/study/gradient-256-none.png \
	docs/encoder/output/high-color/study/gradient-256-none.six \
	docs/encoder/output/high-color/study/gradient-comparison.png \
	docs/encoder/output/high-color/study/gradient-detail.png \
	docs/encoder/output/high-color/study/gradient-high.png \
	docs/encoder/output/high-color/study/gradient-high.six \
	docs/encoder/output/high-color/study/gradient-source.png \
	docs/encoder/output/high-color/study/gray-256-fs.png \
	docs/encoder/output/high-color/study/gray-256-fs.six \
	docs/encoder/output/high-color/study/gray-256-none.png \
	docs/encoder/output/high-color/study/gray-256-none.six \
	docs/encoder/output/high-color/study/gray-comparison.png \
	docs/encoder/output/high-color/study/gray-high.png \
	docs/encoder/output/high-color/study/gray-high.six \
	docs/encoder/output/high-color/study/gray-histogram.png \
	docs/encoder/output/high-color/study/gray-source.png \
	docs/encoder/output/high-color/study/quality-size.png \
	docs/encoder/output/high-color/study/results.json \
	docs/encoder/output/high-color/study/repeat-results.json \
	docs/encoder/output/high-color/study/measurement-generator.py \
	docs/encoder/output/high-color/study/revisit-256-fs.png \
	docs/encoder/output/high-color/study/revisit-256-fs.six \
	docs/encoder/output/high-color/study/revisit-256-none.png \
	docs/encoder/output/high-color/study/revisit-256-none.six \
	docs/encoder/output/high-color/study/revisit-comparison.png \
	docs/encoder/output/high-color/study/revisit-high.png \
	docs/encoder/output/high-color/study/revisit-high.six \
	docs/encoder/output/high-color/study/revisit-source.png \
	docs/encoder/output/high-color/study/snake-256-fs.png \
	docs/encoder/output/high-color/study/snake-256-fs.six \
	docs/encoder/output/high-color/study/snake-256-none.png \
	docs/encoder/output/high-color/study/snake-256-none.six \
	docs/encoder/output/high-color/study/snake-comparison.png \
	docs/encoder/output/high-color/study/snake-detail.png \
	docs/encoder/output/high-color/study/snake-fhd-256-fs.png \
	docs/encoder/output/high-color/study/snake-fhd-256-fs.six \
	docs/encoder/output/high-color/study/snake-fhd-256-none.png \
	docs/encoder/output/high-color/study/snake-fhd-256-none.six \
	docs/encoder/output/high-color/study/snake-fhd-high.png \
	docs/encoder/output/high-color/study/snake-fhd-high.six \
	docs/encoder/output/high-color/study/snake-fhd-source.png \
	docs/encoder/output/high-color/study/snake-high.png \
	docs/encoder/output/high-color/study/snake-high.six \
	docs/encoder/output/high-color/study/snake-source.png \
	docs/encoder/output/high-color/study/summary.csv \
	docs/encoder/output/high-color/study/thread-scaling.png \
	docs/encoder/output/high-color/study/timings.csv \
	docs/encoder/output/high-color/figures/high-color-figures.json \
	docs/encoder/output/high-color/figures/high-color-passes-mobile.svg \
	docs/encoder/output/high-color/figures/high-color-passes-wide.svg \
	docs/encoder/output/high-color/figures/high-color-register-mobile.svg \
	docs/encoder/output/high-color/figures/high-color-register-wide.svg \
	tools/highcolor/study.py \
	tools/highcolor/requirements.txt \
	tools/plot_encoding_mode_figures.py \
	tools/reproduce_encoding_mode_figures.sh \
	docs/encoder/output/or-mode/README.md \
	docs/encoder/output/terminal-macros.md \
	docs/encoder/animation.md \
	docs/encoder/output/drcs.md \
	docs/encoder/palette-construction/fixed-palettes.md \
	docs/cli/clipboard.md \
	docs/cli/diagnostics.md \
	docs/cli/runtime-policy.md \
	docs/loader/patents.md \
	docs/loader/libjpeg.md \
	docs/loader/libwebp.md \
	docs/loader/libtiff.md \
	docs/loader/librsvg.md \
	docs/loader/wic.md \
	docs/loader/coregraphics.md \
	docs/loader/precision-policy.md \
	docs/loader/gdk-pixbuf2.md \
	docs/loader/gd.md \
	docs/loader/quicklook.md \
	docs/loader/gnome-thumbnailer.md \
	docs/encoder/output/or-mode/grid-example/input.png \
	docs/encoder/output/or-mode/grid-example/normal.six \
	docs/encoder/output/or-mode/grid-example/or.six \
	docs/encoder/output/or-mode/grid-example/example.json \
	docs/encoder/output/or-mode/grid-example/grid.svg \
	docs/encoder/output/or-mode/grid-example/grid-mobile.svg \
	tools/ormode/grid_example.py \
	tools/ormode/render_grid_example.py \
	docs/encoder/output/or-mode/encoding-explained.svg \
	docs/encoder/output/or-mode/encoding-explained-mobile.svg \
	tools/ormode/render_explanation.py \
	docs/encoder/output/or-mode/scaling.md \
	docs/encoder/output/or-mode/sweeps/axes.csv \
	docs/encoder/output/or-mode/sweeps/color-curves.png \
	docs/encoder/output/or-mode/sweeps/decode-photos.png \
	docs/encoder/output/or-mode/sweeps/decode.json \
	docs/encoder/output/or-mode/sweeps/encode.json \
	docs/encoder/output/or-mode/sweeps/fullhd-threads.png \
	docs/encoder/output/or-mode/sweeps/inputs.json \
	docs/encoder/output/or-mode/sweeps/metadata.json \
	docs/encoder/output/or-mode/sweeps/summary.json \
	docs/encoder/output/or-mode/sweeps/thread-curves.png \
	docs/encoder/output/or-mode/sweeps/thread-observations.json \
	tools/ormode/decode-bench.c \
	tools/ormode/sweeps.py \
	tools/ormode/render_sweeps.py \
	docs/encoder/output/or-mode/measurements/alpha.json \
	docs/encoder/output/or-mode/measurements/comparison.png \
	docs/encoder/output/or-mode/measurements/controls.json \
	docs/encoder/output/or-mode/measurements/index-boundaries.json \
	docs/encoder/output/or-mode/measurements/inputs.json \
	docs/encoder/output/or-mode/measurements/metadata.json \
	docs/encoder/output/or-mode/measurements/pr133-result.txt \
	docs/encoder/output/or-mode/measurements/preflight.txt \
	docs/encoder/output/or-mode/measurements/qiita-cases.json \
	docs/encoder/output/or-mode/measurements/results.json \
	docs/encoder/output/or-mode/measurements/summary.csv \
	docs/encoder/output/or-mode/measurements/summary.json \
	tools/ormode/download.py \
	tools/ormode/edge_cases.py \
	tools/ormode/indexed-bench.c \
	tools/ormode/measure.py \
	tools/ormode/qiita_cases.py \
	tools/ormode/render.py \
	tools/ormode/requirements.txt \
	docs/sixel-format/figures/encoder-comparison-mobile.svg \
	docs/sixel-format/figures/encoder-comparison-wide.svg \
	docs/sixel-format/figures/node-scheduling-mobile.svg \
	docs/sixel-format/figures/node-scheduling-wide.svg \
	docs/sixel-format/figures/sixel-format-figures.json \
	docs/ci/design.md \
	docs/cli/design-policy.md \
	docs/cli/options.md \
	docs/misc/platforms/README.md \
	docs/misc/platforms/cosmopolitan.md \
	docs/misc/platforms/cygwin-msys.md \
	docs/misc/platforms/emscripten.md \
	docs/misc/platforms/gnu-hurd.md \
	docs/misc/platforms/haiku.md \
	docs/misc/platforms/macos.md \
	docs/misc/platforms/mingw.md \
	docs/misc/platforms/msvc.md \
	docs/misc/platforms/openvms.md \
	docs/misc/platforms/posix-runtimes.md \
	docs/misc/platforms/solaris.md \
	docs/misc/platforms/windows-paths.md \
	docs/misc/platforms/windows.md \
	docs/concepts/color-management/README.md \
	docs/concepts/color-management/figures/pcs-connection-mobile.svg \
	docs/concepts/color-management/figures/pcs-connection-wide.svg \
	docs/concepts/colorspace/README.md \
	docs/concepts/colorspace/primaries-and-white-points.md \
	docs/concepts/colorspace/transfer-functions.md \
	docs/concepts/din99d/README.md \
	docs/concepts/din99d/figures/nearest-color-wide.svg \
	docs/concepts/din99d/figures/nearest-color-mobile.svg \
	docs/concepts/din99d/figures/conversion-route-wide.svg \
	docs/concepts/din99d/figures/conversion-route-mobile.svg \
	docs/concepts/din99d/figures/figures.json \
	tools/plot_din99d_figures.py \
	tools/evaluate.py \
	docs/concepts/colorspace/figures/conversion-routes-mobile.svg \
	docs/concepts/colorspace/figures/conversion-routes-wide.svg \
	docs/concepts/colorspace/figures/figures.json \
	docs/concepts/colorspace/figures/foundations.json \
	docs/concepts/colorspace/figures/pw-barycenter.svg \
	docs/concepts/colorspace/figures/pw-chromaticity.svg \
	docs/concepts/colorspace/figures/pw-gamuts-mobile.svg \
	docs/concepts/colorspace/figures/pw-gamuts-wide.svg \
	docs/concepts/colorspace/figures/pw-misread-mobile.svg \
	docs/concepts/colorspace/figures/pw-misread-wide.svg \
	docs/concepts/colorspace/figures/pw-whites-mobile.svg \
	docs/concepts/colorspace/figures/pw-whites-wide.svg \
	docs/concepts/colorspace/figures/tf-chain-mobile.svg \
	docs/concepts/colorspace/figures/tf-chain-wide.svg \
	docs/concepts/colorspace/figures/tf-curves-mobile.svg \
	docs/concepts/colorspace/figures/tf-curves-wide.svg \
	docs/concepts/colorspace/figures/tf-dark-banding.svg \
	docs/concepts/colorspace/figures/tf-lightness.svg \
	docs/concepts/colorspace/figures/tf-mixing-mobile.svg \
	docs/concepts/colorspace/figures/tf-mixing-wide.svg \
	docs/concepts/colorspace/figures/tf-ramps.svg \
	docs/concepts/colorspace/figures/tf-step-size.svg \
	docs/concepts/colorspace/figures/mixing-mobile.svg \
	docs/concepts/colorspace/figures/mixing-wide.svg \
	docs/concepts/colorspace/figures/primaries-whitepoints-mobile.svg \
	docs/concepts/colorspace/figures/primaries-whitepoints-wide.svg \
	tools/plot_colorspace_figures.py \
	docs/concepts/pixelformat-precision/README.md \
	docs/concepts/pixelformat-precision/measurements/pixelformat-precision-paths.csv \
	docs/concepts/pixelformat-precision/measurements/pixelformat-precision-resize.csv \
	docs/concepts/pixelformat-precision/measurements/pixelformat-precision-resize.png \
	docs/concepts/pixelformat-precision/measurements/pixelformat-precision-run.json \
	docs/concepts/pixelformat.md \
	docs/loader/README.md \
	docs/loader/network-access.md \
	docs/loader/libpng.md \
	docs/loader/color-management/README.md \
	docs/loader/png-color-metadata.md \
	docs/loader/color-management/measurements/cms-default-measurements.json \
	tools/measure_cms_default.py \
	docs/loader/builtin-cms.md \
	docs/loader/color-management/examples/gamut-wide.png \
	docs/loader/color-management/examples/gamut-mobile.png \
	docs/loader/color-management/examples/gamma-wide.png \
	docs/loader/color-management/examples/gamma-mobile.png \
	docs/loader/color-management/examples/examples.json \
	docs/loader/color-management/figures/colorsync-gamma2-wide.png \
	docs/loader/color-management/figures/colorsync-gamma2-mobile.png \
	docs/loader/color-management/measurements/colorsync-gamma2.json \
	docs/loader/color-management/measurements/native-curv-sixel.png \
	docs/loader/color-management/measurements/equivalent-para3-sixel.png \
	docs/loader/color-management/measurements/reference-sixel.png \
	docs/loader/alpha-policy/README.md \
	docs/loader/background-policy.md \
	docs/loader/builtin.md \
	docs/loader/builtin/README.md \
	docs/loader/builtin/bmp.md \
	docs/loader/builtin/gif.md \
	docs/loader/builtin/hdr.md \
	docs/loader/builtin/jpeg.md \
	docs/loader/builtin/netpbm.md \
	docs/loader/builtin/pic.md \
	docs/loader/builtin/png/README.md \
	docs/loader/builtin/png-source-precision.md \
	docs/loader/builtin/png/measurements/png-loaders.json \
	docs/loader/builtin/png/measurements/png-loaders.svg \
	docs/loader/builtin/psd.md \
	docs/loader/builtin/sixel.md \
	docs/loader/builtin/tga.md \
	docs/loader/builtin/figures/bmp.svg \
	docs/loader/builtin/figures/builtin-loader-pipelines.json \
	docs/loader/builtin/figures/gif.svg \
	docs/loader/builtin/figures/hdr.svg \
	docs/loader/builtin/figures/jpeg.svg \
	docs/loader/builtin/figures/netpbm.svg \
	docs/loader/builtin/figures/pic.svg \
	docs/loader/builtin/figures/png.svg \
	docs/loader/builtin/figures/psd.svg \
	docs/loader/builtin/figures/sixel.svg \
	docs/loader/builtin/figures/tga.svg \
	docs/loader/alpha-policy/figures/alpha-policy-comparison.gif \
	docs/loader/alpha-policy/figures/alpha-policy-comparison.png \
	docs/loader/figures/builtin-lineage-mobile.svg \
	docs/loader/figures/builtin-lineage-wide.svg \
	docs/loader/figures/loader-architecture-figures.json \
	docs/loader/figures/loader-chain-mobile.svg \
	docs/loader/figures/loader-chain-wide.svg \
	docs/encoder/preprocessing/crop-resize/README.md \
	docs/encoder/preprocessing/crop-resize/figures/crop-resize-policy-path-mobile.svg \
	docs/encoder/preprocessing/crop-resize/figures/crop-resize-policy-path-wide.svg \
	docs/encoder/preprocessing/crop-resize/measurements/crop-resize-boundary-float.png \
	docs/encoder/preprocessing/crop-resize/measurements/crop-resize-boundary-input.png \
	docs/encoder/preprocessing/crop-resize/measurements/crop-resize-boundary-linear.png \
	docs/encoder/preprocessing/crop-resize/measurements/crop-resize-boundary-preserve.png \
	docs/encoder/preprocessing/crop-resize/measurements/crop-resize-boundary-samples.csv \
	docs/encoder/preprocessing/crop-resize/measurements/crop-resize-quality-checker-delta-e00.png \
	docs/encoder/preprocessing/crop-resize/measurements/crop-resize-quality-checker-ms-ssim.png \
	docs/encoder/preprocessing/crop-resize/measurements/crop-resize-quality-natural-delta-e00.png \
	docs/encoder/preprocessing/crop-resize/measurements/crop-resize-quality-natural-ms-ssim.png \
	docs/encoder/preprocessing/crop-resize/measurements/crop-resize-quality.csv \
	docs/encoder/preprocessing/crop-resize/measurements/crop-resize-run.json \
	docs/encoder/preprocessing/crop-resize/measurements/crop-resize-size.csv \
	docs/encoder/preprocessing/crop-resize/measurements/crop-resize-size.png \
	docs/encoder/preprocessing/crop-resize/measurements/crop-resize-speed.csv \
	docs/encoder/preprocessing/crop-resize/measurements/crop-resize-speed.png \
	docs/encoder/preprocessing/crop-resize/measurements/crop-resize-visual-float-auto-zoom.png \
	docs/encoder/preprocessing/crop-resize/measurements/crop-resize-visual-float-auto.png \
	docs/encoder/preprocessing/crop-resize/measurements/crop-resize-visual-linear-auto-zoom.png \
	docs/encoder/preprocessing/crop-resize/measurements/crop-resize-visual-linear-auto.png \
	docs/encoder/preprocessing/crop-resize/measurements/crop-resize-visual-preserve-auto-zoom.png \
	docs/encoder/preprocessing/crop-resize/measurements/crop-resize-visual-preserve-auto.png \
	docs/encoder/preprocessing/crop-resize/measurements/crop-resize-visual-reference-linear-scalar-zoom.png \
	docs/encoder/preprocessing/crop-resize/measurements/crop-resize-visual-reference-linear-scalar.png \
	docs/encoder/preprocessing/resampling/README.md \
	docs/encoder/preprocessing/resampling/compact-kernels.md \
	docs/encoder/preprocessing/resampling/theory-history.md \
	docs/encoder/preprocessing/resampling/wide-kernels.md \
	docs/encoder/preprocessing/resampling/figures/resampling-coordinate-mapping.png \
	docs/encoder/preprocessing/resampling/figures/resampling-kernel-bicubic.png \
	docs/encoder/preprocessing/resampling/figures/resampling-kernel-bilinear.png \
	docs/encoder/preprocessing/resampling/figures/resampling-kernel-gaussian.png \
	docs/encoder/preprocessing/resampling/figures/resampling-kernel-hamming.png \
	docs/encoder/preprocessing/resampling/figures/resampling-kernel-hanning.png \
	docs/encoder/preprocessing/resampling/figures/resampling-kernel-lanczos2.png \
	docs/encoder/preprocessing/resampling/figures/resampling-kernel-lanczos3.png \
	docs/encoder/preprocessing/resampling/figures/resampling-kernel-lanczos4.png \
	docs/encoder/preprocessing/resampling/figures/resampling-kernel-nearest.png \
	docs/encoder/preprocessing/resampling/figures/resampling-kernel-welsh.png \
	docs/encoder/preprocessing/resampling/figures/resampling-kernels-radius-1.png \
	docs/encoder/preprocessing/resampling/figures/resampling-kernels-wide.png \
	docs/encoder/preprocessing/resampling/figures/resampling-theory-aliasing.png \
	docs/encoder/preprocessing/resampling/figures/resampling-theory-sinc-reconstruction.png \
	docs/encoder/preprocessing/resampling/measurements/resampling-discrete-stencils.csv \
	docs/encoder/preprocessing/resampling/measurements/resampling-discrete-stencils.png \
	docs/encoder/preprocessing/resampling/measurements/resampling-frequency-response-compact.png \
	docs/encoder/preprocessing/resampling/measurements/resampling-frequency-response-wide.png \
	docs/encoder/preprocessing/resampling/measurements/resampling-frequency-response.csv \
	docs/encoder/preprocessing/resampling/measurements/resampling-isotropy-gain.png \
	docs/encoder/preprocessing/resampling/measurements/resampling-isotropy-spread.png \
	docs/encoder/preprocessing/resampling/measurements/resampling-isotropy.csv \
	docs/encoder/preprocessing/resampling/measurements/resampling-moire-bicubic.png \
	docs/encoder/preprocessing/resampling/measurements/resampling-moire-bilinear.png \
	docs/encoder/preprocessing/resampling/measurements/resampling-moire-gaussian.png \
	docs/encoder/preprocessing/resampling/measurements/resampling-moire-hamming.png \
	docs/encoder/preprocessing/resampling/measurements/resampling-moire-hanning.png \
	docs/encoder/preprocessing/resampling/measurements/resampling-moire-input.png \
	docs/encoder/preprocessing/resampling/measurements/resampling-moire-lanczos2.png \
	docs/encoder/preprocessing/resampling/measurements/resampling-moire-lanczos3.png \
	docs/encoder/preprocessing/resampling/measurements/resampling-moire-lanczos4.png \
	docs/encoder/preprocessing/resampling/measurements/resampling-moire-nearest.png \
	docs/encoder/preprocessing/resampling/measurements/resampling-moire-reference.png \
	docs/encoder/preprocessing/resampling/measurements/resampling-moire-welsh.png \
	docs/encoder/preprocessing/resampling/measurements/resampling-moire.csv \
	docs/encoder/preprocessing/resampling/measurements/resampling-natural-bicubic-zoom.png \
	docs/encoder/preprocessing/resampling/measurements/resampling-natural-bilinear-zoom.png \
	docs/encoder/preprocessing/resampling/measurements/resampling-natural-gaussian-zoom.png \
	docs/encoder/preprocessing/resampling/measurements/resampling-natural-hamming-zoom.png \
	docs/encoder/preprocessing/resampling/measurements/resampling-natural-hanning-zoom.png \
	docs/encoder/preprocessing/resampling/measurements/resampling-natural-lanczos2-zoom.png \
	docs/encoder/preprocessing/resampling/measurements/resampling-natural-lanczos3-zoom.png \
	docs/encoder/preprocessing/resampling/measurements/resampling-natural-lanczos4-zoom.png \
	docs/encoder/preprocessing/resampling/measurements/resampling-natural-nearest-zoom.png \
	docs/encoder/preprocessing/resampling/measurements/resampling-natural-reference-zoom.png \
	docs/encoder/preprocessing/resampling/measurements/resampling-natural-welsh-zoom.png \
	docs/encoder/preprocessing/resampling/measurements/resampling-quality-delta-e00.png \
	docs/encoder/preprocessing/resampling/measurements/resampling-quality-ms-ssim.png \
	docs/encoder/preprocessing/resampling/measurements/resampling-quality-size.csv \
	docs/encoder/preprocessing/resampling/measurements/resampling-run.json \
	docs/encoder/preprocessing/resampling/measurements/resampling-size.png \
	docs/encoder/preprocessing/resampling/measurements/resampling-speed.csv \
	docs/encoder/preprocessing/resampling/measurements/resampling-speed.png \
	docs/encoder/palette-construction/clustering-colorspace/README.md \
	docs/encoder/palette-application/working-colorspace/README.md \
	docs/encoder/output/output-colorspace.md \
	docs/encoder/palette-construction/merge-policy/README.md \
	docs/encoder/palette-construction/merge-policy/measurements/merge-policy-comparison.csv \
	docs/encoder/palette-construction/merge-policy/measurements/merge-policy-mechanics.png \
	docs/encoder/palette-construction/merge-policy/measurements/merge-policy-performance.png \
	docs/encoder/palette-construction/merge-policy/measurements/merge-policy-quality.png \
	docs/encoder/palette-construction/merge-policy/measurements/merge-policy-reachability-bimodality.png \
	docs/encoder/palette-construction/merge-policy/measurements/merge-policy-reachability-hotspots.png \
	docs/encoder/palette-construction/merge-policy/measurements/merge-policy-reachability-patch-size.csv \
	docs/encoder/palette-construction/merge-policy/measurements/merge-policy-reachability-run.json \
	docs/encoder/palette-construction/merge-policy/measurements/merge-policy-reachability-summary.csv \
	docs/encoder/palette-construction/merge-policy/measurements/merge-policy-reachability.png \
	docs/encoder/palette-construction/merge-policy/measurements/merge-policy-visual-comparison.png \
	docs/encoder/palette-construction/merge-policy/measurements/merge-policy-run.json \
	docs/encoder/palette-application/working-colorspace/measurements/working-colorspace-comparison.csv \
	docs/encoder/palette-application/working-colorspace/measurements/working-colorspace-performance.png \
	docs/encoder/palette-application/working-colorspace/measurements/working-colorspace-quality.png \
	docs/encoder/palette-application/working-colorspace/measurements/working-colorspace-run.json \
	docs/encoder/palette-construction/clustering-colorspace/measurements/clustering-colorspace-quality.csv \
	docs/encoder/palette-construction/clustering-colorspace/measurements/clustering-colorspace-quality.png \
	docs/encoder/palette-construction/clustering-colorspace/measurements/clustering-colorspace-binning-occupancy.csv \
	docs/encoder/palette-construction/clustering-colorspace/measurements/clustering-colorspace-binning-occupancy.png \
	docs/encoder/palette-construction/clustering-colorspace/measurements/clustering-colorspace-binning-quality.csv \
	docs/encoder/palette-construction/clustering-colorspace/measurements/clustering-colorspace-binning-quality.png \
	docs/encoder/palette-construction/clustering-colorspace/measurements/clustering-colorspace-run.json \
	docs/encoder/palette-construction/clustering-colorspace/measurements/clustering-colorspace-size.csv \
	docs/encoder/palette-construction/clustering-colorspace/measurements/clustering-colorspace-size.png \
	docs/encoder/palette-construction/clustering-colorspace/measurements/clustering-colorspace-speed.csv \
	docs/encoder/palette-construction/clustering-colorspace/measurements/clustering-colorspace-speed.png \
	docs/encoder/palette-construction/clustering-colorspace/threshold-measurements/clustering-binning-threshold-pass-rate.png \
	docs/encoder/palette-construction/clustering-colorspace/threshold-measurements/clustering-binning-threshold-quality.png \
	docs/encoder/palette-construction/clustering-colorspace/threshold-measurements/clustering-binning-threshold-raw.csv \
	docs/encoder/palette-construction/clustering-colorspace/threshold-measurements/clustering-binning-threshold-run.json \
	docs/encoder/palette-construction/clustering-colorspace/threshold-measurements/clustering-binning-threshold-summary.csv \
	docs/encoder/palette-construction/clustering-colorspace/threshold-measurements/clustering-binning-thresholds.csv \
	docs/encoder/palette-construction/cover-policy/README.md \
	docs/encoder/palette-construction/cover-policy/figures/cover-boundary-clamping.svg \
	docs/encoder/palette-construction/cover-policy/figures/cover-convex-hull-3d.svg \
	docs/encoder/palette-construction/cover-policy/figures/cover-flicker-keyframes.svg \
	docs/encoder/palette-construction/cover-policy/figures/cover-flicker.png \
	docs/encoder/palette-construction/cover-policy/figures/cover-hull-projection.svg \
	docs/encoder/palette-construction/cover-policy/figures/cover-policy-figures.json \
	docs/encoder/palette-construction/cover-policy/figures/cover-reachability-limits.svg \
	docs/encoder/palette-construction/cover-policy/figures/cover-rgb-corners.svg \
	docs/encoder/palette-application/clamp-policy.md \
	docs/encoder/palette-construction/snap-policy/README.md \
	docs/encoder/palette-construction/snap-policy/measurements/snap-policy-controls.csv \
	docs/encoder/palette-construction/snap-policy/measurements/snap-policy-controls.png \
	docs/encoder/palette-construction/snap-policy/measurements/snap-policy-fixed-point.png \
	docs/encoder/palette-construction/snap-policy/measurements/snap-policy-fixture-summary.png \
	docs/encoder/palette-construction/snap-policy/measurements/snap-policy-high-k.csv \
	docs/encoder/palette-construction/snap-policy/measurements/snap-policy-high-k-quality.png \
	docs/encoder/palette-construction/snap-policy/measurements/snap-policy-high-k-size-speed.png \
	docs/encoder/palette-construction/snap-policy/measurements/snap-policy-high-k-speed.csv \
	docs/encoder/palette-construction/snap-policy/measurements/snap-policy-quality.csv \
	docs/encoder/palette-construction/snap-policy/measurements/snap-policy-quality.png \
	docs/encoder/palette-construction/snap-policy/measurements/snap-policy-run.json \
	docs/encoder/palette-construction/snap-policy/measurements/snap-policy-size.png \
	docs/encoder/palette-construction/snap-policy/measurements/snap-policy-speed.csv \
	docs/encoder/palette-construction/snap-policy/measurements/snap-policy-speed.png \
	docs/encoder/palette-application/dithering/README.md \
	docs/encoder/palette-application/dithering/perturb/README.md \
	docs/encoder/palette-application/dithering/perturb/01-flat-native.png \
	docs/encoder/palette-application/dithering/perturb/02-flat-4x.png \
	docs/encoder/palette-application/dithering/perturb/03-gray-ramp.png \
	docs/encoder/palette-application/dithering/perturb/04-photo-crop-locations.png \
	docs/encoder/palette-application/dithering/perturb/05-photo-16-2x.png \
	docs/encoder/palette-application/dithering/perturb/05-photo-256-2x.png \
	docs/encoder/palette-application/dithering/perturb/06-color-16-2x.png \
	docs/encoder/palette-application/dithering/perturb/06-color-256-2x.png \
	docs/encoder/palette-application/dithering/perturb/07-strength-4x.png \
	docs/encoder/palette-application/dithering/perturb/08-seeds-4x.png \
	docs/encoder/palette-application/dithering/perturb/bw.pal \
	docs/encoder/palette-application/dithering/perturb/color-16-fs-0.png \
	docs/encoder/palette-application/dithering/perturb/color-16-fs-05.png \
	docs/encoder/palette-application/dithering/perturb/color-16-serp-0.png \
	docs/encoder/palette-application/dithering/perturb/color-16-serp-05.png \
	docs/encoder/palette-application/dithering/perturb/color-16.pal \
	docs/encoder/palette-application/dithering/perturb/color-256-fs-0.png \
	docs/encoder/palette-application/dithering/perturb/color-256-fs-05.png \
	docs/encoder/palette-application/dithering/perturb/color-256-serp-0.png \
	docs/encoder/palette-application/dithering/perturb/color-256-serp-05.png \
	docs/encoder/palette-application/dithering/perturb/color-256.pal \
	docs/encoder/palette-application/dithering/perturb/color-source.png \
	docs/encoder/palette-application/dithering/perturb/g128-fs-0.png \
	docs/encoder/palette-application/dithering/perturb/g128-fs-025.png \
	docs/encoder/palette-application/dithering/perturb/g128-fs-05-seed1.png \
	docs/encoder/palette-application/dithering/perturb/g128-fs-05-seed2.png \
	docs/encoder/palette-application/dithering/perturb/g128-fs-05.png \
	docs/encoder/palette-application/dithering/perturb/g128-fs-1.png \
	docs/encoder/palette-application/dithering/perturb/g128-serp-0.png \
	docs/encoder/palette-application/dithering/perturb/g128-serp-05.png \
	docs/encoder/palette-application/dithering/perturb/g128-source.png \
	docs/encoder/palette-application/dithering/perturb/g32-fs-0.png \
	docs/encoder/palette-application/dithering/perturb/g32-fs-05.png \
	docs/encoder/palette-application/dithering/perturb/g32-serp-0.png \
	docs/encoder/palette-application/dithering/perturb/g32-serp-05.png \
	docs/encoder/palette-application/dithering/perturb/g32-source.png \
	docs/encoder/palette-application/dithering/perturb/g64-fs-0.png \
	docs/encoder/palette-application/dithering/perturb/g64-fs-025.png \
	docs/encoder/palette-application/dithering/perturb/g64-fs-05-seed1.png \
	docs/encoder/palette-application/dithering/perturb/g64-fs-05-seed2.png \
	docs/encoder/palette-application/dithering/perturb/g64-fs-05.png \
	docs/encoder/palette-application/dithering/perturb/g64-fs-1.png \
	docs/encoder/palette-application/dithering/perturb/g64-serp-0.png \
	docs/encoder/palette-application/dithering/perturb/g64-serp-05.png \
	docs/encoder/palette-application/dithering/perturb/g64-source.png \
	docs/encoder/palette-application/dithering/perturb/g85-fs-0.png \
	docs/encoder/palette-application/dithering/perturb/g85-fs-05.png \
	docs/encoder/palette-application/dithering/perturb/g85-serp-0.png \
	docs/encoder/palette-application/dithering/perturb/g85-serp-05.png \
	docs/encoder/palette-application/dithering/perturb/g85-source.png \
	docs/encoder/palette-application/dithering/perturb/manifest.json \
	docs/encoder/palette-application/dithering/perturb/measurements.csv \
	docs/encoder/palette-application/dithering/perturb/photo-16-fs-0.png \
	docs/encoder/palette-application/dithering/perturb/photo-16-fs-05.png \
	docs/encoder/palette-application/dithering/perturb/photo-16-serp-0.png \
	docs/encoder/palette-application/dithering/perturb/photo-16-serp-05.png \
	docs/encoder/palette-application/dithering/perturb/photo-16.pal \
	docs/encoder/palette-application/dithering/perturb/photo-256-fs-0.png \
	docs/encoder/palette-application/dithering/perturb/photo-256-fs-05.png \
	docs/encoder/palette-application/dithering/perturb/photo-256-serp-0.png \
	docs/encoder/palette-application/dithering/perturb/photo-256-serp-05.png \
	docs/encoder/palette-application/dithering/perturb/photo-256.pal \
	docs/encoder/palette-application/dithering/perturb/photo-source.png \
	docs/encoder/palette-application/dithering/perturb/ramp-fs-0.png \
	docs/encoder/palette-application/dithering/perturb/ramp-fs-05.png \
	docs/encoder/palette-application/dithering/perturb/ramp-serp-0.png \
	docs/encoder/palette-application/dithering/perturb/ramp-serp-05.png \
	docs/encoder/palette-application/dithering/perturb/ramp-source.png \
	docs/encoder/palette-application/dithering/perturb/rapsd-curves.csv \
	docs/encoder/palette-application/dithering/perturb/rapsd.png \
	docs/encoder/palette-application/dithering/measurements/dither-policy-delta-e00.png \
	docs/encoder/palette-application/dithering/measurements/dither-policy-ms-ssim.png \
	docs/encoder/palette-application/dithering/measurements/dither-policy-quality.csv \
	docs/encoder/palette-application/dithering/measurements/dither-policy-run.json \
	docs/encoder/palette-application/dithering/measurements/dither-policy-size.csv \
	docs/encoder/palette-application/dithering/measurements/dither-policy-size.png \
	docs/encoder/palette-application/dithering/measurements/dither-policy-speed.csv \
	docs/encoder/palette-application/dithering/measurements/dither-policy-speed.png \
	docs/decoder/dequantization/README.md \
	tools/plot_dequantization_figures.py \
	docs/decoder/dequantization/figures/figures.json \
	docs/decoder/dequantization/figures/position-cases-wide.svg \
	docs/decoder/dequantization/figures/position-cases-mobile.svg \
	docs/decoder/dequantization/figures/rgb-geometry-wide.svg \
	docs/decoder/dequantization/figures/rgb-geometry-mobile.svg \
	docs/decoder/dequantization/figures/palette-clue-wide.svg \
	docs/decoder/dequantization/figures/palette-clue-mobile.svg \
	docs/decoder/dequantization/figures/gradient-clue-wide.svg \
	docs/decoder/dequantization/figures/gradient-clue-mobile.svg \
	docs/decoder/dequantization/figures/full-mobile.svg \
	docs/decoder/dequantization/figures/full-wide.svg \
	docs/decoder/dequantization/figures/light-mobile.svg \
	docs/decoder/dequantization/figures/light-wide.svg \
	docs/decoder/dequantization/figures/none-mobile.svg \
	docs/decoder/dequantization/figures/none-wide.svg \
	docs/decoder/dequantization/figures/selective-mobile.svg \
	docs/decoder/dequantization/figures/selective-wide.svg \
	docs/decoder/pipeline/README.md \
	docs/encoder/output/delta-encoding.md \
	docs/decoder/pipeline/figures/decoder-output-policy.svg \
	docs/decoder/pipeline/figures/decoder-parallel.svg \
	docs/decoder/pipeline/figures/decoder-parser.svg \
	docs/decoder/pipeline/figures/decoder-raster-policy.svg \
	docs/decoder/pipeline/figures/decoder-representations.svg \
	docs/decoder/pipeline/figures/decoding-pipeline-figures.json \
	docs/decoder/pipeline/figures/decoding-pipeline-mobile.svg \
	docs/decoder/pipeline/figures/decoding-pipeline-wide.svg \
	docs/encoder/pipeline/README.md \
	docs/encoder/pipeline/figures/encoding-pipeline-figures.json \
	docs/encoder/pipeline/figures/encoding-pipeline-mobile.svg \
	docs/encoder/pipeline/figures/encoding-pipeline-wide.svg \
	docs/encoder/execution-map/README.md \
	docs/encoder/execution-map/figures/encoder-execution-map-figures.json \
	docs/encoder/execution-map/figures/encoder-scheduler-mobile.svg \
	docs/encoder/execution-map/figures/encoder-scheduler-wide.svg \
	docs/encoder/execution-map/figures/frame-preparation-mobile.svg \
	docs/encoder/execution-map/figures/frame-preparation-wide.svg \
	docs/encoder/execution-map/figures/palette-apply-output-mobile.svg \
	docs/encoder/execution-map/figures/palette-apply-output-wide.svg \
	docs/encoder/execution-map/figures/palette-build-mobile.svg \
	docs/encoder/execution-map/figures/palette-build-wide.svg \
	docs/encoder/palette-construction/pipeline/README.md \
	docs/encoder/palette-construction/pipeline/measurements/palette-pipeline-quality.csv \
	docs/encoder/palette-construction/pipeline/measurements/palette-pipeline-quality.png \
	docs/encoder/palette-construction/pipeline/measurements/palette-pipeline-run.json \
	docs/encoder/palette-construction/pipeline/measurements/palette-pipeline-size.csv \
	docs/encoder/palette-construction/pipeline/measurements/palette-pipeline-size.png \
	docs/encoder/palette-construction/pipeline/measurements/palette-pipeline-speed.csv \
	docs/encoder/palette-construction/pipeline/measurements/palette-pipeline-speed.png \
	docs/encoder/palette-construction/pipeline/suite-measurements/palette-pipeline-suite-quality.csv \
	docs/encoder/palette-construction/pipeline/suite-measurements/palette-pipeline-suite-quality.png \
	docs/encoder/palette-construction/pipeline/suite-measurements/palette-pipeline-suite-run.json \
	docs/encoder/palette-construction/pipeline/suite-measurements/palette-pipeline-suite-size.csv \
	docs/encoder/palette-construction/pipeline/suite-measurements/palette-pipeline-suite-size.png \
	docs/encoder/palette-construction/pipeline/suite-measurements/palette-pipeline-suite-speed-content.png \
	docs/encoder/palette-construction/pipeline/suite-measurements/palette-pipeline-suite-speed-scale.png \
	docs/encoder/palette-construction/pipeline/suite-measurements/palette-pipeline-suite-speed.csv \
	images/measurements/palette-pipeline/a98-gamut-600x450.png \
	images/measurements/palette-pipeline/rare-colors-600x450.png \
	images/measurements/palette-pipeline/smooth-gradient-128x96.png \
	images/measurements/palette-pipeline/smooth-gradient-600x450.png \
	images/measurements/palette-pipeline/smooth-gradient-900x675.png \
	images/measurements/palette-pipeline/smooth-gradient-1920x1080.png \
	docs/encoder/palette-application/lookup-policy/README.md \
	docs/encoder/palette-application/lookup-policy/measurements/lookup-policy-color-error.csv \
	docs/encoder/palette-application/lookup-policy/measurements/lookup-policy-color-error.png \
	docs/encoder/palette-application/lookup-policy/measurements/lookup-policy-acceleration-run.json \
	docs/encoder/palette-application/lookup-policy/measurements/lookup-policy-heckbert-high-k.csv \
	docs/encoder/palette-application/lookup-policy/measurements/lookup-policy-heckbert-high-k.png \
	docs/encoder/palette-application/lookup-policy/measurements/lookup-policy-kmeans-color-error.csv \
	docs/encoder/palette-application/lookup-policy/measurements/lookup-policy-kmeans-color-error.png \
	docs/encoder/palette-application/lookup-policy/measurements/lookup-policy-kmeans-fs-color-error.csv \
	docs/encoder/palette-application/lookup-policy/measurements/lookup-policy-kmeans-fs-color-error.png \
	docs/encoder/palette-application/lookup-policy/measurements/lookup-policy-kmeans-fs-ms-ssim.csv \
	docs/encoder/palette-application/lookup-policy/measurements/lookup-policy-kmeans-fs-ms-ssim.png \
	docs/encoder/palette-application/lookup-policy/measurements/lookup-policy-kmeans-ms-ssim.csv \
	docs/encoder/palette-application/lookup-policy/measurements/lookup-policy-kmeans-ms-ssim.png \
	docs/encoder/palette-application/lookup-policy/measurements/lookup-policy-ms-ssim.csv \
	docs/encoder/palette-application/lookup-policy/measurements/lookup-policy-ms-ssim.png \
	docs/encoder/palette-application/lookup-policy/measurements/lookup-policy-metal-speed.csv \
	docs/encoder/palette-application/lookup-policy/measurements/lookup-policy-metal-speed.png \
	docs/encoder/palette-application/lookup-policy/measurements/lookup-policy-fs-run.json \
	docs/encoder/palette-application/lookup-policy/measurements/lookup-policy-fs-speed.csv \
	docs/encoder/palette-application/lookup-policy/measurements/lookup-policy-fs-speed.png \
	docs/encoder/palette-application/lookup-policy/measurements/lookup-policy-run.json \
	docs/encoder/palette-application/lookup-policy/measurements/lookup-policy-shared-speed.csv \
	docs/encoder/palette-application/lookup-policy/measurements/lookup-policy-shared-speed.png \
	docs/encoder/palette-application/lookup-policy/measurements/lookup-policy-speed.csv \
	docs/encoder/palette-application/lookup-policy/measurements/lookup-policy-speed.png \
	docs/overview.md \
	docs/encoder/precision/README.md \
	docs/encoder/precision/measurements/precision-clustering-colorspace.png \
	docs/encoder/precision/measurements/precision-comparison.csv \
	docs/encoder/precision/measurements/precision-dither.png \
	docs/encoder/precision/measurements/precision-lookup.png \
	docs/encoder/precision/measurements/precision-palette-pipeline.png \
	docs/encoder/precision/measurements/precision-quantizer.png \
	docs/encoder/precision/measurements/precision-run.json \
	docs/encoder/palette-construction/quantization/measurements/quantize-model-quality.csv \
	docs/encoder/palette-construction/quantization/measurements/quantize-model-quality.png \
	docs/encoder/palette-construction/quantization/measurements/quantize-model-run.json \
	docs/encoder/palette-construction/quantization/measurements/quantize-model-size.csv \
	docs/encoder/palette-construction/quantization/measurements/quantize-model-size.png \
	docs/encoder/palette-construction/quantization/measurements/quantize-model-speed.csv \
	docs/encoder/palette-construction/quantization/measurements/quantize-model-speed.png \
	docs/encoder/palette-construction/quantization/README.md \
	docs/quality/measurement-policy.md \
	docs/quality/metrics.md \
	docs/concepts/clustering/README.md \
	docs/concepts/clustering/figures/color-cloud.svg \
	docs/concepts/clustering/figures/figures.json \
	docs/concepts/clustering/figures/kmeans.svg \
	docs/concepts/clustering/figures/mean-vs-max.svg \
	docs/concepts/clustering/figures/median-cut.svg \
	docs/concepts/clustering/figures/palette-results.svg \
	docs/concepts/clustering/figures/voronoi.svg \
	docs/concepts/dithering/README.md \
	docs/concepts/dithering/figures/error-diffusion-kernel.svg \
	docs/concepts/dithering/figures/figures.json \
	docs/concepts/dithering/figures/linear-averaging.svg \
	docs/concepts/dithering/figures/methods-comparison.svg \
	docs/concepts/dithering/figures/ordered-dither.svg \
	docs/concepts/dithering/figures/posterization.svg \
	docs/concepts/dithering/figures/spectra.svg \
	docs/concepts/displays/README.md \
	docs/concepts/displays/figures/ambient-light.svg \
	docs/concepts/displays/figures/color-mixing.svg \
	docs/concepts/displays/figures/epaper-palettes.svg \
	docs/concepts/displays/figures/figures.json \
	docs/concepts/displays/figures/light-paths.svg \
	docs/concepts/displays/figures/pixel-appearance.svg \
	docs/concepts/displays/figures/pixel-density.svg \
	docs/concepts/displays/figures/subpixels.svg \
	docs/concepts/displays/figures/timeline.svg \
	docs/concepts/palettes/README.md \
	docs/concepts/palettes/figures/block-palette.svg \
	docs/concepts/palettes/figures/figures.json \
	docs/concepts/palettes/figures/indexed-image.svg \
	docs/concepts/palettes/figures/palette-sizes.svg \
	docs/concepts/palettes/figures/standard-palettes.svg \
	docs/concepts/palettes/figures/timeline.svg \
	docs/quality/character-cell-perception/README.md \
	docs/quality/character-cell-perception/figures/csf-ratio.svg \
	docs/quality/character-cell-perception/figures/figures.json \
	docs/quality/character-cell-perception/figures/palette-direct.svg \
	docs/quality/character-cell-perception/figures/tile-dithering.svg \
	docs/quality/visual-perception/README.md \
	docs/quality/visual-perception/figures/blind-spot.svg \
	docs/quality/visual-perception/figures/campbell-robson.svg \
	docs/quality/visual-perception/figures/chromatic-adaptation.svg \
	docs/quality/visual-perception/figures/cone-sensitivity.svg \
	docs/quality/visual-perception/figures/contrast-sensitivity.svg \
	docs/quality/visual-perception/figures/dark-adaptation.svg \
	docs/quality/visual-perception/figures/dither-integration.svg \
	docs/quality/visual-perception/figures/eye-anatomy.jpg \
	docs/quality/visual-perception/figures/figures.json \
	docs/quality/visual-perception/figures/light-levels.svg \
	docs/quality/visual-perception/figures/luma-chroma.svg \
	docs/quality/visual-perception/figures/luminosity-functions.svg \
	docs/quality/visual-perception/figures/opponent-channels.svg \
	docs/quality/visual-perception/figures/receptor-density.svg \
	docs/quality/visual-perception/figures/retina-layers.jpg \
	docs/quality/visual-perception/figures/retina-mosaic.jpg \
	docs/quality/visual-perception/figures/viewing-geometry.svg \
	docs/quality/visual-perception/figures/weber-demo.svg \
	docs/testing/builtin-loader-coverage.md \
	docs/testing/guide.md \
	docs/testing/staticcheck.md \
	docs/testing/png-chrm-boundaries.md \
	docs/testing/png-chrm-arithmetic.md \
	docs/writers/png/README.md \
	docs/writers/png/figures/measurements.json \
	docs/writers/png/figures/writer-time.svg \
	docs/writers/png/figures/writer-time.png \
	docs/writers/png/figures/writer-size.svg \
	docs/writers/png/figures/writer-size.png \
	tools/plot_png_writer_measurements.py \
	docs/threading/README.md \
	docs/threading/animation.md \
	docs/threading/decoder.md \
	docs/threading/encoder.md \
	docs/threading/measurement.md \
	docs/threading/measurements/animation-thread4-timeline.png \
	docs/threading/measurements/animation-thread4.jsonl \
	docs/threading/measurements/decoder-size-comparison.csv \
	docs/threading/measurements/decoder-thread-scaling.csv \
	docs/threading/measurements/decoder-thread-scaling.png \
	docs/threading/measurements/decoder-thread8-900x675.jsonl \
	docs/threading/measurements/decoder-thread8-timeline.png \
	docs/threading/measurements/decoder-thread8.jsonl \
	docs/threading/measurements/encoder-thread2-timeline.png \
	docs/threading/measurements/encoder-thread2.jsonl \
	docs/threading/measurements/encoder-thread4-timeline.png \
	docs/threading/measurements/encoder-thread4.jsonl \
	docs/threading/measurements/encoder-thread8-timeline.png \
	docs/threading/measurements/encoder-thread8.jsonl \
	docs/threading/measurements/encoder-thread-scaling.csv \
	docs/threading/measurements/encoder-thread-scaling.png \
	docs/threading/measurements/encoder-color-scaling.csv \
	docs/threading/measurements/encoder-color-scaling.png \
	docs/threading/measurements/thread-budget.csv \
	docs/threading/measurements/thread-budget.png \
	docs/threading/measurements/threading-run.json \
	docs/threading/overview.md \
	openvms/gnv-ar.sh \
	openvms/gnv-cc.sh \
	openvms/gnv-rm.sh \
	openvms/gnv-link-program.sh \
	tools/fuzz-afl-runner.sh \
	tools/fuzz-img2sixel-cli.sh \
	tools/check_clustering_binning_threshold_measurements.py \
	tools/check_clustering_colorspace_measurements.py \
	tools/check_dither_policy_measurements.py \
	tools/check_palette_pipeline_measurements.py \
	tools/check_palette_pipeline_suite_measurements.py \
	tools/check_pixelformat_precision_measurements.py \
	tools/check_precision_measurements.py \
	tools/check_quantize_model_measurements.py \
	tools/check_snap_policy_measurements.py \
	tools/check_threading_measurements.py \
	tools/generate_builtin_loader_coverage.py \
	tools/generate_png_cms_matrix.py \
	tools/generate_palette_pipeline_suite_fixtures.py \
	tools/gen_perl_constants.pl \
	tools/gen_perl_attach.pl \
	tools/check_lookup_policy_measurements.py \
	tools/check_merge_policy_measurements.py \
	tools/palette_pipeline_suite.json \
	tools/plot_lookup_policy_acceleration.py \
	tools/plot_clustering_binning_threshold_measurements.py \
	tools/plot_clustering_colorspace_measurements.py \
	tools/plot_cover_policy_figures.py \
	tools/plot_dither_policy_measurements.py \
	tools/plot_diffusion_perturb_measurements.py \
	tools/plot_decoding_pipeline_figures.py \
	tools/plot_encoder_execution_map_figures.py \
	tools/plot_encoding_pipeline_figures.py \
	tools/plot_sixel_format_figures.py \
	tools/plot_codec_timeline_figures.py \
	tools/plot_terminal_graphics_timeline.py \
	tools/plot_builtin_loader_format_figures.py \
	tools/measure_png_loaders.py \
	tools/bench/png-loader.c \
	tools/plot_loader_architecture_figures.py \
	tools/plot_loader_cms_examples.py \
	tools/plot_colorsync_gamma.py \
	tools/plot_lookup_policy_quality.sh \
	tools/plot_lookup_policy_speed.py \
	tools/plot_merge_policy_measurements.py \
	tools/palette_gradient.py \
	tools/palette-gradient-requirements.txt \
	docs/quality/palette-gradient-analysis.md \
	docs/cli/abort-trace/README.md \
	docs/cli/abort-trace/figures/abort-trace-macos.png \
	docs/cli/correction-suggestions.md \
	docs/cli/prefix-matching.md \
	docs/cli/suboptions.md \
	docs/encoder/output/encode-policy/README.md \
	docs/encoder/output/encode-policy/figures/encode-policy-band-mobile.svg \
	docs/encoder/output/encode-policy/figures/encode-policy-band-wide.svg \
	docs/encoder/output/encode-policy/figures/encode-policy-figures.json \
	docs/encoder/output/encode-policy/figures/encode-policy-overpaint-mobile.svg \
	docs/encoder/output/encode-policy/figures/encode-policy-overpaint-wide.svg \
	docs/encoder/output/encode-policy/measurements/encode-policy-comparison.csv \
	docs/encoder/output/encode-policy/measurements/encode-policy-results.png \
	docs/encoder/output/encode-policy/measurements/encode-policy-run.json \
	docs/encoder/output/encode-policy/measurements/encode-policy-speed.csv \
	docs/encoder/palette-application/dithering/measurements/dither-policy-spectrum-atlas.png \
	docs/encoder/palette-application/dithering/measurements/dither-policy-spectrum-overview.png \
	docs/encoder/palette-application/dithering/measurements/dither-spectrum-curves.csv \
	docs/encoder/palette-application/dithering/measurements/dither-spectrum-run.json \
	docs/encoder/palette-application/dithering/measurements/dither-spectrum-summary.csv \
	docs/encoder/palette-application/lookup-policy/measurements/lookup-policy-kmeans-high-k.csv \
	docs/encoder/palette-application/lookup-policy/measurements/lookup-policy-kmeans-high-k.png \
	docs/encoder/palette-construction/external-palettes.md \
	docs/quality/color-evaluation.md \
	docs/quality/color-difference.md \
	docs/sixel-format/comparison/README.md \
	docs/sixel-format/comparison/figures/codec-timeline-figures.json \
	docs/sixel-format/comparison/figures/codec-timeline-mobile.svg \
	docs/sixel-format/comparison/figures/codec-timeline-wide.svg \
	docs/sixel-format/comparison/figures/diagram-generations-mobile.svg \
	docs/sixel-format/comparison/figures/diagram-generations-wide.svg \
	docs/sixel-format/comparison/figures/diagram-profile-mobile.svg \
	docs/sixel-format/comparison/figures/diagram-profile-wide.svg \
	docs/sixel-format/comparison/figures/diagram-scaling-mobile.svg \
	docs/sixel-format/comparison/figures/diagram-scaling-wide.svg \
	docs/sixel-format/comparison/figures/diagram.png \
	docs/sixel-format/comparison/figures/photo-generations-mobile.svg \
	docs/sixel-format/comparison/figures/photo-generations-wide.svg \
	docs/sixel-format/comparison/figures/photo-profile-mobile.svg \
	docs/sixel-format/comparison/figures/photo-profile-wide.svg \
	docs/sixel-format/comparison/figures/photo-scaling-mobile.svg \
	docs/sixel-format/comparison/figures/photo-scaling-wide.svg \
	docs/sixel-format/comparison/figures/results.json \
	docs/testing/mapfile-parser-coverage.md \
	docs/testing/resampling-coverage.md \
	tests/tools/palette-gradient/run.py \
	tests/tools/palette-gradient/0001_geometry.py \
	tests/tools/palette-gradient/0002_extraction.py \
	tests/tools/palette-gradient/0003_inputs.py \
	tests/tools/palette-gradient/0004_workflow.py \
	tools/plot_palette_pipeline_measurements.py \
	tools/plot_palette_pipeline_suite_measurements.py \
	tools/plot_quality_curve.py \
	tools/plot_pixelformat_precision_measurements.py \
	tools/plot_precision_measurements.py \
	tools/plot_quantize_model_measurements.py \
	tools/plot_snap_policy_measurements.py \
	tools/plot_threading_measurements.py \
	tools/reproduce_lookup_policy_acceleration.sh \
	tools/reproduce_clustering_binning_threshold_measurements.sh \
	tools/reproduce_clustering_colorspace_measurements.sh \
	tools/reproduce_cover_policy_figures.sh \
	tools/reproduce_dither_policy_measurements.sh \
	tools/reproduce_decoding_pipeline_figures.sh \
	tools/reproduce_encoder_execution_map_figures.sh \
	tools/reproduce_encoding_pipeline_figures.sh \
	tools/reproduce_sixel_format_figures.sh \
	tools/reproduce_lookup_policy_measurements.sh \
	tools/reproduce_merge_policy_measurements.sh \
	tools/reproduce_palette_pipeline_measurements.sh \
	tools/reproduce_palette_pipeline_suite_measurements.sh \
	tools/reproduce_pixelformat_precision_measurements.sh \
	tools/reproduce_precision_measurements.sh \
	tools/reproduce_quantize_model_measurements.sh \
	tools/reproduce_snap_policy_measurements.sh \
	tools/reproduce_threading_measurements.sh \
	tools/snap_policy_suite.json \
	tools/timeline.py

# Let make propagate the caller's job limit and jobserver to recursive builds.
# Do not infer a default from MAKEFLAGS: some make versions omit explicit -j1,
# so absence of a jobs flag must still mean serial execution.
if OPENVMS_GNV
# GNV make loses the following libtool object target when recursive makes are
# run with the silent flag.  Keep build recursion serial there, and force
# Automake's verbose recipes so generated prerequisite recipes do not poison
# the following libtool object target.
# Test sharding is controlled separately by OPENVMS_TEST_JOBS.
LS_OPENVMS_BUILD_MAKEFLAGS = -j1 V=1
else
LS_SILENT_MAKEFLAGS = -s
endif
AM_MAKEFLAGS = $(LS_SILENT_MAKEFLAGS) $(LS_OPENVMS_BUILD_MAKEFLAGS)

# Automake/libtool may require a top-level ltmain.sh in DIST_COMMON even when
# AC_CONFIG_AUX_DIR points at build-aux/.  Generate it from the canonical copy
# so distcheck can always stage distribution files.
ltmain.sh: $(top_srcdir)/build-aux/ltmain.sh
	$(AM_V_GEN)$(INSTALL_DATA) $(top_srcdir)/build-aux/ltmain.sh $@


.PHONY: docs install-quicklook-extension uninstall-quicklook-extension \
	fuzz-cli-img2sixel fuzz-cli-img2sixel-parser \
	fuzz-cli-img2sixel-boundary fuzz-cli-img2sixel-pathenv \
	fuzz-cli-img2sixel-numeric fuzz-cli-img2sixel-all \
	fuzz-loader-builtin fuzz-run-loader-builtin \
	fuzz-run-loader-builtin-formats \
	fuzz-run-loader-coregraphics \
	fuzz-run-loader-wic \
	fuzz-run-struct-push-set \
	fuzz-run-struct-full-set \
	check-python check-ruby check-perl check-php \
	installcheck-local

docs:
	$(AM_V_GEN)$(top_srcdir)/tools/build-docs \
		--output-dir "$(abs_top_builddir)/site" \
		--work-dir "$(abs_top_builddir)/.docs-build"

check-python:
	$(MAKE) -C tests check-python

check-ruby:
	$(MAKE) -C tests check-ruby

check-perl:
	$(MAKE) -C tests check-perl

check-php:
if HAVE_PHP
	$(MAKE) -C php check
	$(MAKE) -C tests check-php
else
	@echo "PHP bindings are not enabled; skipping PHP tests"
endif

installcheck-local:
	@set -eu; \
	if test '@cross_compiling@' = 'yes'; then \
		echo "installcheck: skipped for cross-compiling builds"; \
		exit 0; \
	fi; \
	prefix='@prefix@'; \
	exec_prefix='@exec_prefix@'; \
	bindir='@bindir@'; \
	libexecdir='@libexecdir@'; \
	libdir='@libdir@'; \
	if test "$$prefix" = "NONE"; then \
		prefix='/usr/local'; \
	fi; \
	if test "$$exec_prefix" = "NONE"; then \
		exec_prefix='${prefix}'; \
	fi; \
	eval "exec_prefix=$$exec_prefix"; \
	eval "bindir=$$bindir"; \
	eval "libexecdir=$$libexecdir"; \
	eval "libdir=$$libdir"; \
	install_root=''; \
	if test -n "$${SIXEL_INSTALLCHECK_DESTDIR-}"; then \
		install_root="$${SIXEL_INSTALLCHECK_DESTDIR}"; \
	elif test -n "$${DESTDIR-}"; then \
		install_root="$${DESTDIR}"; \
	fi; \
	install_bindir="$$install_root$$bindir"; \
	install_libexecdir="$$install_root$$libexecdir"; \
	install_libdir="$$install_root$$libdir"; \
	$(MAKE) $(AM_MAKEFLAGS) -C tests installcheck-local \
		SIXEL_INSTALLCHECK=1 \
		SIXEL_INSTALL_BINDIR="$$install_bindir" \
		SIXEL_INSTALL_LIBEXECDIR="$$install_libexecdir" \
		SIXEL_INSTALL_LIBDIR="$$install_libdir"

FUZZ_AFL ?= afl-fuzz
FUZZ_TIMEOUT ?= 0
FUZZ_AFL_WORKERS ?= 1
FUZZ_AFL_MODE ?= normal
FUZZ_AFL_CMPLOG_RUNNER ?=
FUZZ_AFL_EXTRA_OPTS ?=
FUZZ_INPUT_DIR ?= $(top_srcdir)/fuzz/corpus/img2sixel-cli
FUZZ_OUTPUT_DIR ?= $(abs_top_builddir)/afl-out/img2sixel
FUZZ_IMG2SIXEL ?= $(abs_top_builddir)/converters/img2sixel$(EXEEXT)
FUZZ_IMG2SIXEL_BASE_ARGS ?=
FUZZ_IMG2SIXEL_MODE ?= persistent
FUZZ_IMG2SIXEL_PERSISTENT ?= $(abs_top_builddir)/.tmp/fuzz-img2sixel-afl-persistent$(EXEEXT)
FUZZ_IMG2SIXEL_RUNNER ?= $(FUZZ_IMG2SIXEL_PERSISTENT)
FUZZ_IMG2SIXEL_CLI_PARSER_PERSISTENT ?= $(abs_top_builddir)/.tmp/fuzz-img2sixel-cli-parser-afl-persistent$(EXEEXT)
FUZZ_IMG2SIXEL_CLI_BOUNDARY_PERSISTENT ?= $(abs_top_builddir)/.tmp/fuzz-img2sixel-cli-boundary-afl-persistent$(EXEEXT)
FUZZ_IMG2SIXEL_CLI_PATHENV_PERSISTENT ?= $(abs_top_builddir)/.tmp/fuzz-img2sixel-cli-pathenv-afl-persistent$(EXEEXT)
FUZZ_IMG2SIXEL_CLI_NUMERIC_PERSISTENT ?= $(abs_top_builddir)/.tmp/fuzz-img2sixel-cli-numeric-afl-persistent$(EXEEXT)
FUZZ_IMG2SIXEL_EMBED_CPPFLAGS = \
	-DHAVE_CONFIG_H \
	-DBUILD_IMG2SIXEL \
	-DHAVE_COMPLETION_EMBED_H \
	-DLIBSIXEL_IMG2SIXEL_EMBEDDED \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_builddir)/converters \
	-I$(top_srcdir)/converters \
	-I$(top_builddir)/include \
	-I$(top_srcdir)/include
FUZZ_IMG2SIXEL_EMBED_SRCS = \
	$(top_srcdir)/converters/img2sixel.c \
	$(top_srcdir)/converters/completion_utils.c \
	$(top_srcdir)/converters/compat.c \
	$(top_srcdir)/converters/path.c \
	$(top_srcdir)/converters/cli.c \
	$(top_srcdir)/converters/malloc_stub.c \
	$(top_srcdir)/converters/aborttrace.c
FUZZ_IMG2SIXEL_EMBED_LIBS = $(top_builddir)/src/libsixel.la $(LIBS)
if HAVE_ABORT_TRACE
FUZZ_IMG2SIXEL_EMBED_LIBS += $(ABORTTRACE_LIBS)
endif
FUZZ_BUILTIN_CORPUS ?= $(top_srcdir)/images
FUZZ_BUILTIN_ARGS ?=
FUZZ_BUILTIN_FORMATS = png gif jpeg pnm sixel hdr psd pic bmp
FUZZ_STRUCT_TARGETS = \
	png-chunk \
	png-apng \
	png-icc-gamma \
	png-plte-trns \
	jpeg-icc \
	pnm-ascii \
	psd-resource \
	gif-block \
	gif-lzw-codes \
	sixel-stream \
	hdr-header \
	pic-structure \
	bmp-dib \
	bmp-rle-bitfields \
	mapfile-spec \
	mapfile-palette \
	option-arglist \
	loader-order
FUZZ_STRUCT_CORPUS ?= $(FUZZ_BUILTIN_CORPUS)/struct
FUZZ_STRUCT_ARGS ?= $(FUZZ_BUILTIN_ARGS)

install-quicklook-extension:
	@HAVE_APPLE_QUICKLOOK_EXTENSION_TRUE@$(MAKE) -C quicklook-extension install-quicklook-extension
	@@HAVE_APPLE_QUICKLOOK_EXTENSION_FALSE@{ echo "Quick Look extension is not enabled in this build" >&2; exit 1; }

uninstall-quicklook-extension:
	@HAVE_APPLE_QUICKLOOK_EXTENSION_TRUE@$(MAKE) -C quicklook-extension uninstall-quicklook-extension
	@@HAVE_APPLE_QUICKLOOK_EXTENSION_FALSE@{ echo "Quick Look extension is not enabled in this build" >&2; exit 1; }

$(FUZZ_IMG2SIXEL_PERSISTENT): $(top_srcdir)/fuzz/fuzz-img2sixel-afl-persistent.c $(top_builddir)/src/libsixel.la
	@set -eu; \
	mkdir -p "$(dir $@)"; \
	$(LIBTOOL) --mode=link $(CC) $(CPPFLAGS) $(CFLAGS) \
		-I$(top_builddir)/include \
		-I$(top_srcdir)/include \
		-o "$@" "$(top_srcdir)/fuzz/fuzz-img2sixel-afl-persistent.c" \
		$(top_builddir)/src/libsixel.la $(LIBS)

$(FUZZ_IMG2SIXEL_CLI_PARSER_PERSISTENT): $(top_srcdir)/fuzz/fuzz-img2sixel-cli-parser-afl-persistent.c $(FUZZ_IMG2SIXEL) $(top_builddir)/src/libsixel.la
	@set -eu; \
	mkdir -p "$(dir $@)"; \
	$(LIBTOOL) --mode=link $(CC) $(CPPFLAGS) $(CFLAGS) \
		$(FUZZ_IMG2SIXEL_EMBED_CPPFLAGS) \
		-o "$@" "$(top_srcdir)/fuzz/fuzz-img2sixel-cli-parser-afl-persistent.c" \
		$(FUZZ_IMG2SIXEL_EMBED_SRCS) \
		$(FUZZ_IMG2SIXEL_EMBED_LIBS)

$(FUZZ_IMG2SIXEL_CLI_BOUNDARY_PERSISTENT): $(top_srcdir)/fuzz/fuzz-img2sixel-cli-boundary-afl-persistent.c $(FUZZ_IMG2SIXEL) $(top_builddir)/src/libsixel.la
	@set -eu; \
	mkdir -p "$(dir $@)"; \
	$(LIBTOOL) --mode=link $(CC) $(CPPFLAGS) $(CFLAGS) \
		$(FUZZ_IMG2SIXEL_EMBED_CPPFLAGS) \
		-o "$@" "$(top_srcdir)/fuzz/fuzz-img2sixel-cli-boundary-afl-persistent.c" \
		$(FUZZ_IMG2SIXEL_EMBED_SRCS) \
		$(FUZZ_IMG2SIXEL_EMBED_LIBS)

$(FUZZ_IMG2SIXEL_CLI_PATHENV_PERSISTENT): $(top_srcdir)/fuzz/fuzz-img2sixel-cli-pathenv-afl-persistent.c $(FUZZ_IMG2SIXEL) $(top_builddir)/src/libsixel.la
	@set -eu; \
	mkdir -p "$(dir $@)"; \
	$(LIBTOOL) --mode=link $(CC) $(CPPFLAGS) $(CFLAGS) \
		$(FUZZ_IMG2SIXEL_EMBED_CPPFLAGS) \
		-o "$@" "$(top_srcdir)/fuzz/fuzz-img2sixel-cli-pathenv-afl-persistent.c" \
		$(FUZZ_IMG2SIXEL_EMBED_SRCS) \
		$(FUZZ_IMG2SIXEL_EMBED_LIBS)

$(FUZZ_IMG2SIXEL_CLI_NUMERIC_PERSISTENT): $(top_srcdir)/fuzz/fuzz-img2sixel-cli-numeric-afl-persistent.c $(FUZZ_IMG2SIXEL) $(top_builddir)/src/libsixel.la
	@set -eu; \
	mkdir -p "$(dir $@)"; \
	$(LIBTOOL) --mode=link $(CC) $(CPPFLAGS) $(CFLAGS) \
		$(FUZZ_IMG2SIXEL_EMBED_CPPFLAGS) \
		-o "$@" "$(top_srcdir)/fuzz/fuzz-img2sixel-cli-numeric-afl-persistent.c" \
		$(FUZZ_IMG2SIXEL_EMBED_SRCS) \
		$(FUZZ_IMG2SIXEL_EMBED_LIBS)

fuzz-cli-img2sixel: $(FUZZ_IMG2SIXEL) $(FUZZ_IMG2SIXEL_PERSISTENT)
	@set -eu; \
	if [ ! -x "$(top_srcdir)/tools/fuzz-afl-runner.sh" ]; then \
		echo "error: AFL runner wrapper is missing: tools/fuzz-afl-runner.sh" >&2; \
		exit 1; \
	fi; \
	if [ "$(FUZZ_IMG2SIXEL_MODE)" = "legacy" ]; then \
		if [ ! -x "$(FUZZ_IMG2SIXEL)" ]; then \
			echo "error: img2sixel binary was not found: $(FUZZ_IMG2SIXEL)" >&2; \
			exit 1; \
		fi; \
		if [ ! -r "$(top_srcdir)/tools/fuzz-img2sixel-cli.sh" ]; then \
			echo "error: fuzz wrapper is missing: tools/fuzz-img2sixel-cli.sh" >&2; \
			exit 1; \
		fi; \
		echo "fuzz-cli-img2sixel: mode=legacy base args='$(FUZZ_IMG2SIXEL_BASE_ARGS)'"; \
		FUZZ_AFL_WORKERS="$(FUZZ_AFL_WORKERS)" \
		FUZZ_AFL_MODE="$(FUZZ_AFL_MODE)" \
		FUZZ_AFL_CMPLOG_RUNNER="$(FUZZ_AFL_CMPLOG_RUNNER)" \
		FUZZ_AFL_EXTRA_OPTS="$(FUZZ_AFL_EXTRA_OPTS)" \
		FUZZ_IMG2SIXEL="$(FUZZ_IMG2SIXEL)" \
		FUZZ_IMG2SIXEL_BASE_ARGS="$(FUZZ_IMG2SIXEL_BASE_ARGS)" \
		"$(top_srcdir)/tools/fuzz-afl-runner.sh" \
			"fuzz-cli-img2sixel" "$(FUZZ_AFL)" "$(FUZZ_INPUT_DIR)" \
			"$(FUZZ_OUTPUT_DIR)" "$(FUZZ_TIMEOUT)" -- \
			"$(SHELL)" "$(top_srcdir)/tools/fuzz-img2sixel-cli.sh" @@; \
		exit 0; \
	fi; \
	fuzz_runner="$(FUZZ_IMG2SIXEL_RUNNER)"; \
	if [ -f "$$fuzz_runner" ] && head -n 1 "$$fuzz_runner" | grep -q '^#!'; then \
		fuzz_runner_candidate="$$(dirname "$$fuzz_runner")/.libs/$$(basename "$$fuzz_runner")"; \
		if [ -x "$$fuzz_runner_candidate" ]; then \
			fuzz_runner="$$fuzz_runner_candidate"; \
		fi; \
	fi; \
	if [ ! -x "$$fuzz_runner" ]; then \
		echo "error: persistent harness was not found: $$fuzz_runner" >&2; \
		exit 1; \
	fi; \
	if [ -d "$(abs_top_builddir)/src/.libs" ]; then \
		LD_LIBRARY_PATH="$(abs_top_builddir)/src/.libs$${LD_LIBRARY_PATH+:$$LD_LIBRARY_PATH}"; \
		export LD_LIBRARY_PATH; \
	fi; \
	if [ -n "$(FUZZ_IMG2SIXEL_BASE_ARGS)" ]; then \
		echo "fuzz-cli-img2sixel: note: FUZZ_IMG2SIXEL_BASE_ARGS is ignored in persistent mode"; \
	fi; \
	echo "fuzz-cli-img2sixel: mode=persistent harness='$$fuzz_runner'"; \
	FUZZ_AFL_WORKERS="$(FUZZ_AFL_WORKERS)" \
	FUZZ_AFL_MODE="$(FUZZ_AFL_MODE)" \
	FUZZ_AFL_CMPLOG_RUNNER="$(FUZZ_AFL_CMPLOG_RUNNER)" \
	FUZZ_AFL_EXTRA_OPTS="$(FUZZ_AFL_EXTRA_OPTS)" \
	"$(top_srcdir)/tools/fuzz-afl-runner.sh" \
		"fuzz-cli-img2sixel" "$(FUZZ_AFL)" "$(FUZZ_INPUT_DIR)" \
		"$(FUZZ_OUTPUT_DIR)" "$(FUZZ_TIMEOUT)" -- \
		"$$fuzz_runner"

fuzz-cli-img2sixel-parser: $(FUZZ_IMG2SIXEL) $(FUZZ_IMG2SIXEL_CLI_PARSER_PERSISTENT)
	@$(MAKE) $(AM_MAKEFLAGS) fuzz-cli-img2sixel \
		FUZZ_IMG2SIXEL_MODE='persistent' \
		FUZZ_IMG2SIXEL_RUNNER='$(FUZZ_IMG2SIXEL_CLI_PARSER_PERSISTENT)' \
		FUZZ_OUTPUT_DIR='$(FUZZ_OUTPUT_DIR)/parser'

fuzz-cli-img2sixel-boundary: $(FUZZ_IMG2SIXEL) $(FUZZ_IMG2SIXEL_CLI_BOUNDARY_PERSISTENT)
	@$(MAKE) $(AM_MAKEFLAGS) fuzz-cli-img2sixel \
		FUZZ_IMG2SIXEL_MODE='persistent' \
		FUZZ_IMG2SIXEL_RUNNER='$(FUZZ_IMG2SIXEL_CLI_BOUNDARY_PERSISTENT)' \
		FUZZ_OUTPUT_DIR='$(FUZZ_OUTPUT_DIR)/boundary'

fuzz-cli-img2sixel-pathenv: $(FUZZ_IMG2SIXEL) $(FUZZ_IMG2SIXEL_CLI_PATHENV_PERSISTENT)
	@$(MAKE) $(AM_MAKEFLAGS) fuzz-cli-img2sixel \
		FUZZ_IMG2SIXEL_MODE='persistent' \
		FUZZ_IMG2SIXEL_RUNNER='$(FUZZ_IMG2SIXEL_CLI_PATHENV_PERSISTENT)' \
		FUZZ_OUTPUT_DIR='$(FUZZ_OUTPUT_DIR)/pathenv'

fuzz-cli-img2sixel-numeric: $(FUZZ_IMG2SIXEL) $(FUZZ_IMG2SIXEL_CLI_NUMERIC_PERSISTENT)
	@$(MAKE) $(AM_MAKEFLAGS) fuzz-cli-img2sixel \
		FUZZ_IMG2SIXEL_MODE='persistent' \
		FUZZ_IMG2SIXEL_RUNNER='$(FUZZ_IMG2SIXEL_CLI_NUMERIC_PERSISTENT)' \
		FUZZ_OUTPUT_DIR='$(FUZZ_OUTPUT_DIR)/numeric'

fuzz-cli-img2sixel-all: fuzz-cli-img2sixel fuzz-cli-img2sixel-parser fuzz-cli-img2sixel-boundary fuzz-cli-img2sixel-pathenv fuzz-cli-img2sixel-numeric

fuzz-loader-builtin:
	@COND_FUZZ_TRUE@$(MAKE) -C fuzz $(AM_MAKEFLAGS) fuzz-loader-builtin-libfuzzer$(EXEEXT)
	@@COND_FUZZ_FALSE@{ echo "error: fuzz targets are disabled. reconfigure with --enable-fuzz" >&2; exit 1; }

fuzz-run-loader-builtin:
	@COND_FUZZ_TRUE@$(MAKE) -C fuzz $(AM_MAKEFLAGS) run-loader-builtin \
		FUZZ_BUILTIN_CORPUS='$(FUZZ_BUILTIN_CORPUS)' \
		FUZZ_BUILTIN_ARGS='$(FUZZ_BUILTIN_ARGS)'
	@@COND_FUZZ_FALSE@{ echo "error: fuzz targets are disabled. reconfigure with --enable-fuzz" >&2; exit 1; }

fuzz-run-loader-builtin-%:
	@COND_FUZZ_TRUE@$(MAKE) -C fuzz $(AM_MAKEFLAGS) run-loader-builtin-$* \
		FUZZ_BUILTIN_CORPUS='$(FUZZ_BUILTIN_CORPUS)' \
		FUZZ_BUILTIN_ARGS='$(FUZZ_BUILTIN_ARGS)'
	@@COND_FUZZ_FALSE@{ echo "error: fuzz targets are disabled. reconfigure with --enable-fuzz" >&2; exit 1; }

fuzz-run-loader-builtin-formats:
	@COND_FUZZ_TRUE@$(MAKE) -C fuzz $(AM_MAKEFLAGS) run-loader-builtin-formats \
		FUZZ_BUILTIN_CORPUS='$(FUZZ_BUILTIN_CORPUS)' \
		FUZZ_BUILTIN_ARGS='$(FUZZ_BUILTIN_ARGS)'
	@@COND_FUZZ_FALSE@{ echo "error: fuzz targets are disabled. reconfigure with --enable-fuzz" >&2; exit 1; }

fuzz-run-loader-coregraphics:
	@COND_FUZZ_TRUE@$(MAKE) -C src $(AM_MAKEFLAGS) libsixel.la
	@COND_FUZZ_TRUE@$(MAKE) -C fuzz $(AM_MAKEFLAGS) run-loader-coregraphics \
		FUZZ_COMPONENT_CORPUS='$(FUZZ_BUILTIN_CORPUS)/component' \
		FUZZ_COMPONENT_ARGS='$(FUZZ_COMPONENT_ARGS)'
	@@COND_FUZZ_FALSE@{ echo "error: fuzz targets are disabled. reconfigure with --enable-fuzz" >&2; exit 1; }

fuzz-run-loader-wic:
	@COND_FUZZ_TRUE@$(MAKE) -C src $(AM_MAKEFLAGS) libsixel.la
	@COND_FUZZ_TRUE@$(MAKE) -C fuzz $(AM_MAKEFLAGS) run-loader-wic \
		FUZZ_COMPONENT_CORPUS='$(FUZZ_BUILTIN_CORPUS)/component' \
		FUZZ_COMPONENT_ARGS='$(FUZZ_COMPONENT_ARGS)'
	@@COND_FUZZ_FALSE@{ echo "error: fuzz targets are disabled. reconfigure with --enable-fuzz" >&2; exit 1; }

fuzz-run-struct-%:
	@COND_FUZZ_TRUE@$(MAKE) -C fuzz $(AM_MAKEFLAGS) run-struct-$* \
		FUZZ_STRUCT_CORPUS='$(FUZZ_STRUCT_CORPUS)' \
		FUZZ_STRUCT_ARGS='$(FUZZ_STRUCT_ARGS)'
	@@COND_FUZZ_FALSE@{ echo "error: fuzz targets are disabled. reconfigure with --enable-fuzz" >&2; exit 1; }

fuzz-run-struct-push-set:
	@COND_FUZZ_TRUE@$(MAKE) -C fuzz $(AM_MAKEFLAGS) run-struct-push-set \
		FUZZ_STRUCT_CORPUS='$(FUZZ_STRUCT_CORPUS)' \
		FUZZ_STRUCT_ARGS='$(FUZZ_STRUCT_ARGS)'
	@@COND_FUZZ_FALSE@{ echo "error: fuzz targets are disabled. reconfigure with --enable-fuzz" >&2; exit 1; }

fuzz-run-struct-full-set:
	@COND_FUZZ_TRUE@$(MAKE) -C fuzz $(AM_MAKEFLAGS) run-struct-full-set \
		FUZZ_STRUCT_CORPUS='$(FUZZ_STRUCT_CORPUS)' \
		FUZZ_STRUCT_ARGS='$(FUZZ_STRUCT_ARGS)'
	@@COND_FUZZ_FALSE@{ echo "error: fuzz targets are disabled. reconfigure with --enable-fuzz" >&2; exit 1; }

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

srcdir = @srcdir@
VPATH = @srcdir@

# pkgconfig
pkgconfigdir = @pkgconfigdir@
pkgconfig_DATA = $(builddir)/libsixel.pc

CLEANFILES = ltmain.sh $(builddir)/package.json $(builddir)/*.log

# Build a selected group of subdirectories in parallel after src has completed.
#
# Execution flow for recursive all/check with SUBDIRS containing '.':
#   1) include -> src -> .
#   2) At '.', run phase2-all from all-am (this Makefile).
#   3) Remaining subdirs are still visited for compatibility, but most become no-op
#      because their all targets are already up-to-date.
#
# For "--enable-amalgamated-tools --disable-amalgamated-lib", run one-time
# amalgamation preparation before launching parallel phase2 members. This avoids
# races where multiple directories try to generate amalgamation/sixel.c.
.PHONY: \
	prepare-amalgamation-all \
	prepare-amalgamation-check \
	phase1-all \
	phase2-all \
	phase3-all \
	phase2-members-all \
	phase3-members-all \
	all-src-lib \
	all-src-libobjs \
	maybe-all-src-lib \
	all-converters-objs \
	all-assessment-objs \
	all-converters \
	all-tools \
	all-assessment \
	all-gdk-pixbuf-loader \
	all-quicklook-extension \
	all-wic \
	all-ruby \
	all-php \
	all-python \
	openvms-all \
	staticcheck

# GNV make can finish Automake's full all-recursive traversal with status 1
# after all child targets have succeeded.  The OpenVMS CI job calls this
# explicit target to run the already-serialized phased build directly.
openvms-all: config.h
	$(MAKE) -C include $(AM_MAKEFLAGS) all
	$(MAKE) $(AM_MAKEFLAGS) all-am

PHASE2_ALL_TARGETS = \
	all-converters-objs \
	all-assessment-objs \
	all-tools \
	all-python

PHASE3_ALL_TARGETS = \
	all-converters \
	all-assessment \
	all-tools \
	all-python
if ENABLE_GDK_PIXBUF_LOADER
PHASE2_ALL_TARGETS += all-gdk-pixbuf-loader
PHASE3_ALL_TARGETS += all-gdk-pixbuf-loader
endif
if HAVE_APPLE_QUICKLOOK_EXTENSION
PHASE2_ALL_TARGETS += all-quicklook-extension
PHASE3_ALL_TARGETS += all-quicklook-extension
endif
if HAVE_WICCODEC
PHASE2_ALL_TARGETS += all-wic
PHASE3_ALL_TARGETS += all-wic
endif
if HAVE_RUBY
PHASE2_ALL_TARGETS += all-ruby
PHASE3_ALL_TARGETS += all-ruby
endif
if HAVE_PHP
PHASE2_ALL_TARGETS += all-php
PHASE3_ALL_TARGETS += all-php
endif

prepare-amalgamation-all:
if WANT_AMALGAMATED_TOOLS_ONLY
	$(MAKE) -C converters $(AM_MAKEFLAGS) completion_embed.h
	$(MAKE) -C amalgamation $(AM_MAKEFLAGS) sixel.c
endif

prepare-amalgamation-check: prepare-amalgamation-all

# Build libsixel.la in phase1 so phase2/phase3 workers only consume it.
#
# This avoids concurrent relink attempts from multiple phase2 members when
# recursive check/all are launched with parallel jobs.
phase1-all: prepare-amalgamation-all all-src-lib

phase2-members-all: $(PHASE2_ALL_TARGETS)

phase3-members-all: $(PHASE3_ALL_TARGETS)

phase2-all: phase1-all
	$(MAKE) $(AM_MAKEFLAGS) LS_PHASED_ALL=yes phase2-members-all
	$(MAKE) $(AM_MAKEFLAGS) LS_PHASED_ALL=yes phase3-all

phase3-all:
	$(MAKE) $(AM_MAKEFLAGS) phase3-members-all

all-src-libobjs:
	$(MAKE) -C src $(AM_MAKEFLAGS) libobjs

all-src-lib: all-src-libobjs
	$(MAKE) -C src $(AM_MAKEFLAGS) libsixel.la
	$(MAKE) -C src $(AM_MAKEFLAGS) libsixel-import-lib-alias

# Language binding targets can also be invoked directly.  During the top-level
# phased build, phase1-all has already built libsixel.la; rerunning all-src-lib
# there is redundant and especially expensive on OpenVMS because libobjs is
# intentionally expanded into one recursive make per object.
maybe-all-src-lib:
	@if test "x$(LS_PHASED_ALL)" = xyes; then \
	  :; \
	else \
	  $(MAKE) $(AM_MAKEFLAGS) all-src-lib; \
	fi

all-converters-objs:
	$(MAKE) -C converters $(AM_MAKEFLAGS) prelink-objs

all-assessment-objs:
	$(MAKE) -C assessment $(AM_MAKEFLAGS) prelink-objs

all-converters:
	$(MAKE) -C converters $(AM_MAKEFLAGS) all

all-tools:
	$(MAKE) -C tools $(AM_MAKEFLAGS) all

all-assessment:
	$(MAKE) -C assessment $(AM_MAKEFLAGS) all

all-gdk-pixbuf-loader:
	$(MAKE) -C gdk-pixbuf-loader $(AM_MAKEFLAGS) all

all-quicklook-extension:
	$(MAKE) -C quicklook-extension $(AM_MAKEFLAGS) all

all-wic:
	$(MAKE) -C wic $(AM_MAKEFLAGS) all

# Build libsixel.la at the top level before entering ruby/python.
# This prevents parallel recursion from racing on src/libsixel.la when
# language packaging targets run at the same time.
all-ruby: maybe-all-src-lib
	$(MAKE) -C ruby $(AM_MAKEFLAGS) all

all-php: maybe-all-src-lib
	$(MAKE) -C php $(AM_MAKEFLAGS) all

# Keep the src/lib link step serialized at the top level to avoid
# duplicate "make -C src all" recursion from language bindings.
all-python: maybe-all-src-lib
	$(MAKE) -C python $(AM_MAKEFLAGS) all

all-am: phase2-all
	cp $(builddir)/package.json.in $(builddir)/package.json


check-am: prepare-amalgamation-check
	@:

staticcheck:
	$(MAKE) -C tests $(AM_MAKEFLAGS) staticcheck

install-data-local:
	@:

uninstall-local:
	@:

register:
	-regsvr32 -s "$(shell cygpath -w $(builddir)/wic/.libs/libwicsixel.dll)"
	@echo Registered: .libs/libwicsixel.dll

unregister:
	-regsvr32 -u -s "$(shell cygpath -w $(builddir)/wic/.libs/libwicsixel.dll)"
	@echo Unregistered: .libs/libwicsixel.dll

dist-windows: dist-zip
	mkdir -p @PACKAGE_NAME@-@PACKAGE_VERSION@/bin-windows
	cp converters/*.exe @PACKAGE_NAME@-@PACKAGE_VERSION@/bin-windows/
	zip -u @PACKAGE_NAME@-@PACKAGE_VERSION@.zip @PACKAGE_NAME@-@PACKAGE_VERSION@/bin-windows/*.exe
	@host_triplet=''; \
	for confopt in $$(./config.status --config); do \
	  case "$$confopt" in \
	    --host=*) host_triplet=$${confopt#--host=} ;; \
	  esac; \
	done; \
	mv -v @PACKAGE_NAME@-@PACKAGE_VERSION@.zip \
	  @PACKAGE_NAME@-@PACKAGE_VERSION@-$$host_triplet.zip
