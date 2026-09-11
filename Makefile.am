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
EXTRA_DIST = LICENSE build-aux/depcomp \
	mkdocs.yml \
	m4/ax_count_cpus.m4 \
	package.json.in.in \
	build-aux/lso-tap-driver.sh.in \
	build-aux/read-check-test-list.sh \
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
	docs/assets/javascripts/mathjax.js \
	docs/platform-support.md \
	docs/project-history.md \
	docs/sixel-format.md \
	docs/functionality/or-mode.md \
	docs/functionality/or-mode/measurements/alpha.json \
	docs/functionality/or-mode/measurements/comparison.png \
	docs/functionality/or-mode/measurements/controls.json \
	docs/functionality/or-mode/measurements/index-boundaries.json \
	docs/functionality/or-mode/measurements/inputs.json \
	docs/functionality/or-mode/measurements/metadata.json \
	docs/functionality/or-mode/measurements/pr133-result.txt \
	docs/functionality/or-mode/measurements/preflight.txt \
	docs/functionality/or-mode/measurements/qiita-cases.json \
	docs/functionality/or-mode/measurements/results.json \
	docs/functionality/or-mode/measurements/summary.csv \
	docs/functionality/or-mode/measurements/summary.json \
	tools/ormode/download.py \
	tools/ormode/edge_cases.py \
	tools/ormode/indexed-bench.c \
	tools/ormode/measure.py \
	tools/ormode/qiita_cases.py \
	tools/ormode/render.py \
	tools/ormode/requirements.txt \
	docs/sixel-format-figures/encoder-comparison-mobile.svg \
	docs/sixel-format-figures/encoder-comparison-wide.svg \
	docs/sixel-format-figures/node-scheduling-mobile.svg \
	docs/sixel-format-figures/node-scheduling-wide.svg \
	docs/sixel-format-figures/sixel-format-figures.json \
	docs/ci/design.md \
	docs/cli/design-policy.md \
	docs/misc/platforms/README.md \
	docs/misc/platforms/cosmopolitan.md \
	docs/misc/platforms/cygwin-msys.md \
	docs/misc/platforms/emscripten.md \
	docs/misc/platforms/haiku.md \
	docs/misc/platforms/macos.md \
	docs/misc/platforms/mingw.md \
	docs/misc/platforms/msvc.md \
	docs/misc/platforms/openvms.md \
	docs/misc/platforms/posix-runtimes.md \
	docs/misc/platforms/solaris.md \
	docs/misc/platforms/windows-paths.md \
	docs/misc/platforms/windows.md \
	docs/concepts/colorspace.md \
	docs/concepts/pixelformat-precision.md \
	docs/concepts/pixelformat-precision/measurements/pixelformat-precision-paths.csv \
	docs/concepts/pixelformat-precision/measurements/pixelformat-precision-resize.csv \
	docs/concepts/pixelformat-precision/measurements/pixelformat-precision-resize.png \
	docs/concepts/pixelformat-precision/measurements/pixelformat-precision-run.json \
	docs/concepts/pixelformat.md \
	docs/loader/README.md \
	docs/loader/color-management.md \
	docs/loader/cms-default-measurements.json \
	tools/measure_cms_default.py \
	docs/loader/builtin-cms.md \
	docs/loader/cms-examples/gamut-wide.png \
	docs/loader/cms-examples/gamut-mobile.png \
	docs/loader/cms-examples/gamma-wide.png \
	docs/loader/cms-examples/gamma-mobile.png \
	docs/loader/cms-examples/examples.json \
	docs/loader/alpha-policy.md \
	docs/loader/background-policy.md \
	docs/loader/builtin.md \
	docs/loader/builtin/README.md \
	docs/loader/builtin/bmp.md \
	docs/loader/builtin/gif.md \
	docs/loader/builtin/hdr.md \
	docs/loader/builtin/jpeg.md \
	docs/loader/builtin/netpbm.md \
	docs/loader/builtin/pic.md \
	docs/loader/builtin/png.md \
	docs/loader/builtin/measurements/png-loaders.json \
	docs/loader/builtin/measurements/png-loaders.svg \
	docs/loader/builtin/psd.md \
	docs/loader/builtin/sixel.md \
	docs/loader/builtin/tga.md \
	docs/loader/builtin/webp.md \
	docs/loader/builtin/pipeline-figures/bmp.svg \
	docs/loader/builtin/pipeline-figures/builtin-loader-pipelines.json \
	docs/loader/builtin/pipeline-figures/gif.svg \
	docs/loader/builtin/pipeline-figures/hdr.svg \
	docs/loader/builtin/pipeline-figures/jpeg.svg \
	docs/loader/builtin/pipeline-figures/netpbm.svg \
	docs/loader/builtin/pipeline-figures/pic.svg \
	docs/loader/builtin/pipeline-figures/png.svg \
	docs/loader/builtin/pipeline-figures/psd.svg \
	docs/loader/builtin/pipeline-figures/sixel.svg \
	docs/loader/builtin/pipeline-figures/tga.svg \
	docs/loader/builtin/pipeline-figures/webp.svg \
	docs/loader/assets/alpha-policy-comparison.gif \
	docs/loader/assets/alpha-policy-comparison.png \
	docs/loader/loader-architecture-figures/builtin-lineage-mobile.svg \
	docs/loader/loader-architecture-figures/builtin-lineage-wide.svg \
	docs/loader/loader-architecture-figures/loader-architecture-figures.json \
	docs/loader/loader-architecture-figures/loader-chain-mobile.svg \
	docs/loader/loader-architecture-figures/loader-chain-wide.svg \
	docs/functionality/crop-resize.md \
	docs/functionality/crop-resize/figures/crop-resize-policy-path-mobile.svg \
	docs/functionality/crop-resize/figures/crop-resize-policy-path-wide.svg \
	docs/functionality/crop-resize/measurements/crop-resize-boundary-float.png \
	docs/functionality/crop-resize/measurements/crop-resize-boundary-input.png \
	docs/functionality/crop-resize/measurements/crop-resize-boundary-linear.png \
	docs/functionality/crop-resize/measurements/crop-resize-boundary-preserve.png \
	docs/functionality/crop-resize/measurements/crop-resize-boundary-samples.csv \
	docs/functionality/crop-resize/measurements/crop-resize-quality-checker-delta-e00.png \
	docs/functionality/crop-resize/measurements/crop-resize-quality-checker-ms-ssim.png \
	docs/functionality/crop-resize/measurements/crop-resize-quality-natural-delta-e00.png \
	docs/functionality/crop-resize/measurements/crop-resize-quality-natural-ms-ssim.png \
	docs/functionality/crop-resize/measurements/crop-resize-quality.csv \
	docs/functionality/crop-resize/measurements/crop-resize-run.json \
	docs/functionality/crop-resize/measurements/crop-resize-size.csv \
	docs/functionality/crop-resize/measurements/crop-resize-size.png \
	docs/functionality/crop-resize/measurements/crop-resize-speed.csv \
	docs/functionality/crop-resize/measurements/crop-resize-speed.png \
	docs/functionality/crop-resize/measurements/crop-resize-visual-float-auto-zoom.png \
	docs/functionality/crop-resize/measurements/crop-resize-visual-float-auto.png \
	docs/functionality/crop-resize/measurements/crop-resize-visual-linear-auto-zoom.png \
	docs/functionality/crop-resize/measurements/crop-resize-visual-linear-auto.png \
	docs/functionality/crop-resize/measurements/crop-resize-visual-preserve-auto-zoom.png \
	docs/functionality/crop-resize/measurements/crop-resize-visual-preserve-auto.png \
	docs/functionality/crop-resize/measurements/crop-resize-visual-reference-linear-scalar-zoom.png \
	docs/functionality/crop-resize/measurements/crop-resize-visual-reference-linear-scalar.png \
	docs/functionality/resampling.md \
	docs/functionality/resampling/compact-kernels.md \
	docs/functionality/resampling/theory-history.md \
	docs/functionality/resampling/wide-kernels.md \
	docs/functionality/resampling/figures/resampling-coordinate-mapping.png \
	docs/functionality/resampling/figures/resampling-kernel-bicubic.png \
	docs/functionality/resampling/figures/resampling-kernel-bilinear.png \
	docs/functionality/resampling/figures/resampling-kernel-gaussian.png \
	docs/functionality/resampling/figures/resampling-kernel-hamming.png \
	docs/functionality/resampling/figures/resampling-kernel-hanning.png \
	docs/functionality/resampling/figures/resampling-kernel-lanczos2.png \
	docs/functionality/resampling/figures/resampling-kernel-lanczos3.png \
	docs/functionality/resampling/figures/resampling-kernel-lanczos4.png \
	docs/functionality/resampling/figures/resampling-kernel-nearest.png \
	docs/functionality/resampling/figures/resampling-kernel-welsh.png \
	docs/functionality/resampling/figures/resampling-kernels-radius-1.png \
	docs/functionality/resampling/figures/resampling-kernels-wide.png \
	docs/functionality/resampling/figures/resampling-theory-aliasing.png \
	docs/functionality/resampling/figures/resampling-theory-sinc-reconstruction.png \
	docs/functionality/resampling/measurements/resampling-discrete-stencils.csv \
	docs/functionality/resampling/measurements/resampling-discrete-stencils.png \
	docs/functionality/resampling/measurements/resampling-frequency-response-compact.png \
	docs/functionality/resampling/measurements/resampling-frequency-response-wide.png \
	docs/functionality/resampling/measurements/resampling-frequency-response.csv \
	docs/functionality/resampling/measurements/resampling-isotropy-gain.png \
	docs/functionality/resampling/measurements/resampling-isotropy-spread.png \
	docs/functionality/resampling/measurements/resampling-isotropy.csv \
	docs/functionality/resampling/measurements/resampling-moire-bicubic.png \
	docs/functionality/resampling/measurements/resampling-moire-bilinear.png \
	docs/functionality/resampling/measurements/resampling-moire-gaussian.png \
	docs/functionality/resampling/measurements/resampling-moire-hamming.png \
	docs/functionality/resampling/measurements/resampling-moire-hanning.png \
	docs/functionality/resampling/measurements/resampling-moire-input.png \
	docs/functionality/resampling/measurements/resampling-moire-lanczos2.png \
	docs/functionality/resampling/measurements/resampling-moire-lanczos3.png \
	docs/functionality/resampling/measurements/resampling-moire-lanczos4.png \
	docs/functionality/resampling/measurements/resampling-moire-nearest.png \
	docs/functionality/resampling/measurements/resampling-moire-reference.png \
	docs/functionality/resampling/measurements/resampling-moire-welsh.png \
	docs/functionality/resampling/measurements/resampling-moire.csv \
	docs/functionality/resampling/measurements/resampling-natural-bicubic-zoom.png \
	docs/functionality/resampling/measurements/resampling-natural-bilinear-zoom.png \
	docs/functionality/resampling/measurements/resampling-natural-gaussian-zoom.png \
	docs/functionality/resampling/measurements/resampling-natural-hamming-zoom.png \
	docs/functionality/resampling/measurements/resampling-natural-hanning-zoom.png \
	docs/functionality/resampling/measurements/resampling-natural-lanczos2-zoom.png \
	docs/functionality/resampling/measurements/resampling-natural-lanczos3-zoom.png \
	docs/functionality/resampling/measurements/resampling-natural-lanczos4-zoom.png \
	docs/functionality/resampling/measurements/resampling-natural-nearest-zoom.png \
	docs/functionality/resampling/measurements/resampling-natural-reference-zoom.png \
	docs/functionality/resampling/measurements/resampling-natural-welsh-zoom.png \
	docs/functionality/resampling/measurements/resampling-quality-delta-e00.png \
	docs/functionality/resampling/measurements/resampling-quality-ms-ssim.png \
	docs/functionality/resampling/measurements/resampling-quality-size.csv \
	docs/functionality/resampling/measurements/resampling-run.json \
	docs/functionality/resampling/measurements/resampling-size.png \
	docs/functionality/resampling/measurements/resampling-speed.csv \
	docs/functionality/resampling/measurements/resampling-speed.png \
	docs/functionality/clustering-colorspace.md \
	docs/functionality/working-colorspace.md \
	docs/functionality/merge-policy.md \
	docs/functionality/merge-policies/measurements/merge-policy-comparison.csv \
	docs/functionality/merge-policies/measurements/merge-policy-mechanics.png \
	docs/functionality/merge-policies/measurements/merge-policy-performance.png \
	docs/functionality/merge-policies/measurements/merge-policy-quality.png \
	docs/functionality/merge-policies/measurements/merge-policy-reachability-bimodality.png \
	docs/functionality/merge-policies/measurements/merge-policy-reachability-hotspots.png \
	docs/functionality/merge-policies/measurements/merge-policy-reachability-patch-size.csv \
	docs/functionality/merge-policies/measurements/merge-policy-reachability-run.json \
	docs/functionality/merge-policies/measurements/merge-policy-reachability-summary.csv \
	docs/functionality/merge-policies/measurements/merge-policy-reachability.png \
	docs/functionality/merge-policies/measurements/merge-policy-visual-comparison.png \
	docs/functionality/merge-policies/measurements/merge-policy-run.json \
	docs/functionality/working-colorspaces/measurements/working-colorspace-comparison.csv \
	docs/functionality/working-colorspaces/measurements/working-colorspace-performance.png \
	docs/functionality/working-colorspaces/measurements/working-colorspace-quality.png \
	docs/functionality/working-colorspaces/measurements/working-colorspace-run.json \
	docs/functionality/clustering-color-spaces/measurements/clustering-colorspace-quality.csv \
	docs/functionality/clustering-color-spaces/measurements/clustering-colorspace-quality.png \
	docs/functionality/clustering-color-spaces/measurements/clustering-colorspace-binning-occupancy.csv \
	docs/functionality/clustering-color-spaces/measurements/clustering-colorspace-binning-occupancy.png \
	docs/functionality/clustering-color-spaces/measurements/clustering-colorspace-binning-quality.csv \
	docs/functionality/clustering-color-spaces/measurements/clustering-colorspace-binning-quality.png \
	docs/functionality/clustering-color-spaces/measurements/clustering-colorspace-run.json \
	docs/functionality/clustering-color-spaces/measurements/clustering-colorspace-size.csv \
	docs/functionality/clustering-color-spaces/measurements/clustering-colorspace-size.png \
	docs/functionality/clustering-color-spaces/measurements/clustering-colorspace-speed.csv \
	docs/functionality/clustering-color-spaces/measurements/clustering-colorspace-speed.png \
	docs/functionality/clustering-color-spaces/threshold-measurements/clustering-binning-threshold-pass-rate.png \
	docs/functionality/clustering-color-spaces/threshold-measurements/clustering-binning-threshold-quality.png \
	docs/functionality/clustering-color-spaces/threshold-measurements/clustering-binning-threshold-raw.csv \
	docs/functionality/clustering-color-spaces/threshold-measurements/clustering-binning-threshold-run.json \
	docs/functionality/clustering-color-spaces/threshold-measurements/clustering-binning-threshold-summary.csv \
	docs/functionality/clustering-color-spaces/threshold-measurements/clustering-binning-thresholds.csv \
	docs/functionality/cover-policy.md \
	docs/functionality/cover-policy-figures/cover-boundary-clamping.svg \
	docs/functionality/cover-policy-figures/cover-convex-hull-3d.svg \
	docs/functionality/cover-policy-figures/cover-flicker-keyframes.svg \
	docs/functionality/cover-policy-figures/cover-flicker.png \
	docs/functionality/cover-policy-figures/cover-hull-projection.svg \
	docs/functionality/cover-policy-figures/cover-policy-figures.json \
	docs/functionality/cover-policy-figures/cover-reachability-limits.svg \
	docs/functionality/cover-policy-figures/cover-rgb-corners.svg \
	docs/functionality/snap-policy.md \
	docs/functionality/snap-policies/measurements/snap-policy-controls.csv \
	docs/functionality/snap-policies/measurements/snap-policy-controls.png \
	docs/functionality/snap-policies/measurements/snap-policy-fixed-point.png \
	docs/functionality/snap-policies/measurements/snap-policy-fixture-summary.png \
	docs/functionality/snap-policies/measurements/snap-policy-high-k.csv \
	docs/functionality/snap-policies/measurements/snap-policy-high-k-quality.png \
	docs/functionality/snap-policies/measurements/snap-policy-high-k-size-speed.png \
	docs/functionality/snap-policies/measurements/snap-policy-high-k-speed.csv \
	docs/functionality/snap-policies/measurements/snap-policy-quality.csv \
	docs/functionality/snap-policies/measurements/snap-policy-quality.png \
	docs/functionality/snap-policies/measurements/snap-policy-run.json \
	docs/functionality/snap-policies/measurements/snap-policy-size.png \
	docs/functionality/snap-policies/measurements/snap-policy-speed.csv \
	docs/functionality/snap-policies/measurements/snap-policy-speed.png \
	docs/functionality/dithering.md \
	docs/functionality/dither-policies/measurements/dither-policy-delta-e00.png \
	docs/functionality/dither-policies/measurements/dither-policy-ms-ssim.png \
	docs/functionality/dither-policies/measurements/dither-policy-quality.csv \
	docs/functionality/dither-policies/measurements/dither-policy-run.json \
	docs/functionality/dither-policies/measurements/dither-policy-size.csv \
	docs/functionality/dither-policies/measurements/dither-policy-size.png \
	docs/functionality/dither-policies/measurements/dither-policy-speed.csv \
	docs/functionality/dither-policies/measurements/dither-policy-speed.png \
	docs/functionality/decoding-pipeline.md \
	docs/functionality/decoding-pipeline-figures/decoder-output-policy.svg \
	docs/functionality/decoding-pipeline-figures/decoder-parallel.svg \
	docs/functionality/decoding-pipeline-figures/decoder-parser.svg \
	docs/functionality/decoding-pipeline-figures/decoder-raster-policy.svg \
	docs/functionality/decoding-pipeline-figures/decoder-representations.svg \
	docs/functionality/decoding-pipeline-figures/decoding-pipeline-figures.json \
	docs/functionality/decoding-pipeline-figures/decoding-pipeline-mobile.svg \
	docs/functionality/decoding-pipeline-figures/decoding-pipeline-wide.svg \
	docs/functionality/encoding-pipeline.md \
	docs/functionality/encoding-pipeline-figures/encoding-pipeline-figures.json \
	docs/functionality/encoding-pipeline-figures/encoding-pipeline-mobile.svg \
	docs/functionality/encoding-pipeline-figures/encoding-pipeline-wide.svg \
	docs/functionality/encoder-execution-map.md \
	docs/functionality/encoder-execution-map-figures/encoder-execution-map-figures.json \
	docs/functionality/encoder-execution-map-figures/encoder-scheduler-mobile.svg \
	docs/functionality/encoder-execution-map-figures/encoder-scheduler-wide.svg \
	docs/functionality/encoder-execution-map-figures/frame-preparation-mobile.svg \
	docs/functionality/encoder-execution-map-figures/frame-preparation-wide.svg \
	docs/functionality/encoder-execution-map-figures/palette-apply-output-mobile.svg \
	docs/functionality/encoder-execution-map-figures/palette-apply-output-wide.svg \
	docs/functionality/encoder-execution-map-figures/palette-build-mobile.svg \
	docs/functionality/encoder-execution-map-figures/palette-build-wide.svg \
	docs/functionality/palette-pipeline.md \
	docs/functionality/palette-pipeline/measurements/palette-pipeline-quality.csv \
	docs/functionality/palette-pipeline/measurements/palette-pipeline-quality.png \
	docs/functionality/palette-pipeline/measurements/palette-pipeline-run.json \
	docs/functionality/palette-pipeline/measurements/palette-pipeline-size.csv \
	docs/functionality/palette-pipeline/measurements/palette-pipeline-size.png \
	docs/functionality/palette-pipeline/measurements/palette-pipeline-speed.csv \
	docs/functionality/palette-pipeline/measurements/palette-pipeline-speed.png \
	docs/functionality/palette-pipeline/suite-measurements/palette-pipeline-suite-quality.csv \
	docs/functionality/palette-pipeline/suite-measurements/palette-pipeline-suite-quality.png \
	docs/functionality/palette-pipeline/suite-measurements/palette-pipeline-suite-run.json \
	docs/functionality/palette-pipeline/suite-measurements/palette-pipeline-suite-size.csv \
	docs/functionality/palette-pipeline/suite-measurements/palette-pipeline-suite-size.png \
	docs/functionality/palette-pipeline/suite-measurements/palette-pipeline-suite-speed-content.png \
	docs/functionality/palette-pipeline/suite-measurements/palette-pipeline-suite-speed-scale.png \
	docs/functionality/palette-pipeline/suite-measurements/palette-pipeline-suite-speed.csv \
	images/measurements/palette-pipeline/a98-gamut-600x450.png \
	images/measurements/palette-pipeline/rare-colors-600x450.png \
	images/measurements/palette-pipeline/smooth-gradient-128x96.png \
	images/measurements/palette-pipeline/smooth-gradient-600x450.png \
	images/measurements/palette-pipeline/smooth-gradient-900x675.png \
	images/measurements/palette-pipeline/smooth-gradient-1920x1080.png \
	docs/functionality/lookup-policy.md \
	docs/functionality/lookup-policies/measurements/lookup-policy-color-error.csv \
	docs/functionality/lookup-policies/measurements/lookup-policy-color-error.png \
	docs/functionality/lookup-policies/measurements/lookup-policy-acceleration-run.json \
	docs/functionality/lookup-policies/measurements/lookup-policy-heckbert-high-k.csv \
	docs/functionality/lookup-policies/measurements/lookup-policy-heckbert-high-k.png \
	docs/functionality/lookup-policies/measurements/lookup-policy-kmeans-color-error.csv \
	docs/functionality/lookup-policies/measurements/lookup-policy-kmeans-color-error.png \
	docs/functionality/lookup-policies/measurements/lookup-policy-kmeans-fs-color-error.csv \
	docs/functionality/lookup-policies/measurements/lookup-policy-kmeans-fs-color-error.png \
	docs/functionality/lookup-policies/measurements/lookup-policy-kmeans-fs-ms-ssim.csv \
	docs/functionality/lookup-policies/measurements/lookup-policy-kmeans-fs-ms-ssim.png \
	docs/functionality/lookup-policies/measurements/lookup-policy-kmeans-ms-ssim.csv \
	docs/functionality/lookup-policies/measurements/lookup-policy-kmeans-ms-ssim.png \
	docs/functionality/lookup-policies/measurements/lookup-policy-ms-ssim.csv \
	docs/functionality/lookup-policies/measurements/lookup-policy-ms-ssim.png \
	docs/functionality/lookup-policies/measurements/lookup-policy-metal-speed.csv \
	docs/functionality/lookup-policies/measurements/lookup-policy-metal-speed.png \
	docs/functionality/lookup-policies/measurements/lookup-policy-fs-run.json \
	docs/functionality/lookup-policies/measurements/lookup-policy-fs-speed.csv \
	docs/functionality/lookup-policies/measurements/lookup-policy-fs-speed.png \
	docs/functionality/lookup-policies/measurements/lookup-policy-run.json \
	docs/functionality/lookup-policies/measurements/lookup-policy-shared-speed.csv \
	docs/functionality/lookup-policies/measurements/lookup-policy-shared-speed.png \
	docs/functionality/lookup-policies/measurements/lookup-policy-speed.csv \
	docs/functionality/lookup-policies/measurements/lookup-policy-speed.png \
	docs/functionality/overview.md \
	docs/functionality/precision.md \
	docs/functionality/precision/measurements/precision-clustering-colorspace.png \
	docs/functionality/precision/measurements/precision-comparison.csv \
	docs/functionality/precision/measurements/precision-dither.png \
	docs/functionality/precision/measurements/precision-lookup.png \
	docs/functionality/precision/measurements/precision-palette-pipeline.png \
	docs/functionality/precision/measurements/precision-quantizer.png \
	docs/functionality/precision/measurements/precision-run.json \
	docs/functionality/quantize-models/measurements/quantize-model-quality.csv \
	docs/functionality/quantize-models/measurements/quantize-model-quality.png \
	docs/functionality/quantize-models/measurements/quantize-model-run.json \
	docs/functionality/quantize-models/measurements/quantize-model-size.csv \
	docs/functionality/quantize-models/measurements/quantize-model-size.png \
	docs/functionality/quantize-models/measurements/quantize-model-speed.csv \
	docs/functionality/quantize-models/measurements/quantize-model-speed.png \
	docs/functionality/quantization.md \
	docs/quality/measurement-policy.md \
	docs/testing/builtin-loader-coverage.md \
	docs/testing/guide.md \
	docs/writers/png.md \
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
	tools/plot_decoding_pipeline_figures.py \
	tools/plot_encoder_execution_map_figures.py \
	tools/plot_encoding_pipeline_figures.py \
	tools/plot_sixel_format_figures.py \
	tools/plot_builtin_loader_format_figures.py \
	tools/measure_png_loaders.py \
	tools/bench/png-loader.c \
	tools/plot_loader_architecture_figures.py \
	tools/plot_loader_cms_examples.py \
	tools/plot_lookup_policy_quality.sh \
	tools/plot_lookup_policy_speed.py \
	tools/plot_merge_policy_measurements.py \
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

# Honor user-supplied make parallel flags while providing a CPU-based default.
#
# Do not rewrite MAKEFLAGS in the Makefile. GNU make computes runtime parallel
# behavior before variable expansion, so mutating MAKEFLAGS here can produce
# inconsistent results. Instead, append the default via AM_MAKEFLAGS only when
# no user-provided job control is visible in MFLAGS/MAKEFLAGS.
# Accept both explicit jobs options and GNU make jobserver tokens.
LS_USER_JOBS = $(filter -j% --jobs --jobs=% --jobserver-auth=% --jobserver-fds=% --jobserver-style=%,$(MFLAGS) $(MAKEFLAGS))
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
AM_MAKEFLAGS = $(LS_SILENT_MAKEFLAGS) $(LS_OPENVMS_BUILD_MAKEFLAGS) $(if $(filter 0,$(MAKELEVEL)),$(if $(LS_USER_JOBS),,@LS_DEFAULT_MAKEFLAGS@),)

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
