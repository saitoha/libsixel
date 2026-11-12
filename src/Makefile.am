
srcdir = @srcdir@
top_srcdir = @top_srcdir@
VPATH = @srcdir@
prefix = @prefix@
exec_prefix = @exec_prefix@
top_builddir = @top_builddir@
builddir = @builddir@

SIXEL_SHEBANG_FILE = @SIXEL_SHEBANG_FILE@
SIXEL_SHEBANG_HELPER = $(top_srcdir)/build-aux/sixel-apply-shebang.sh

# Metal text is translated into build-tree C literals, not runtime files.
SIXEL_METAL_INPUTS = \
    $(srcdir)/dither-policy-lso2-gpu.metal \
    $(srcdir)/dither-policy-fs-gpu.metal \
    $(srcdir)/dither-policy-atkinson-gpu.metal \
    $(srcdir)/dither-policy-jajuni-gpu.metal \
    $(srcdir)/dither-policy-stucki-gpu.metal \
    $(srcdir)/dither-policy-burkes-gpu.metal \
    $(srcdir)/dither-policy-sierra1-gpu.metal \
    $(srcdir)/dither-policy-sierra2-gpu.metal \
    $(srcdir)/dither-policy-sierra3-gpu.metal \
    $(srcdir)/retained-plane-gpu.metal \
    $(srcdir)/lookup-policy-ordered-gpu.metal \
    $(srcdir)/lookup-policy-none-fractional-gpu.metal \
    $(srcdir)/lookup-policy-eytzinger-fractional-gpu.metal \
    $(srcdir)/lookup-policy-gridlut-fractional-gpu.metal \
    $(srcdir)/lookup-policy-blockcull-fractional-gpu.metal \
    $(srcdir)/lookup-policy-bitlut-fractional-gpu.metal \
    $(srcdir)/lookup-policy-fhedt-fractional-gpu.metal \
    $(srcdir)/lookup-policy-delaunay-fractional-gpu.metal \
    $(srcdir)/lookup-policy-vptree-gpu.metal \
    $(srcdir)/lookup-policy-rbc-gpu.metal \
    $(srcdir)/lookup-policy-mahalanobis-gpu.metal \
    $(srcdir)/lookup-policy-certlut-gpu.metal \
    $(srcdir)/gpu-palette-legacy-types.metal \
    $(srcdir)/gpu-palette-legacy-query.metal \
    $(srcdir)/lookup-policy-gridlut-gpu.metal \
    $(srcdir)/lookup-policy-delaunay-gpu.metal \
    $(srcdir)/lookup-policy-eytzinger-gpu.metal \
    $(srcdir)/lookup-policy-fhedt-gpu-query.metal \
    $(srcdir)/gpu-palette-legacy-pixel.metal \
    $(srcdir)/gpu-palette-legacy-apply.metal \
    $(srcdir)/gpu-dequant-fast4.metal \
    $(srcdir)/lookup-policy-none-gpu.metal \
    $(srcdir)/lookup-policy-blockcull-gpu.metal \
    $(srcdir)/lookup-policy-fhedt-gpu.metal \
    $(srcdir)/lookup-policy-bitlut-gpu.metal \
    $(srcdir)/gpu-palette-fused-prefix.metal \
    $(srcdir)/gpu-palette-fused-suffix.metal
SIXEL_METAL_FRAGMENTS = $(patsubst $(srcdir)/%.metal,generated-metal/%-shader.inc.h,$(SIXEL_METAL_INPUTS))
# The "vulkan.lookup-rgb8.v1" programs. Every GLSL chunk is embedded as
# text: the runtime composes consumer prefix, lookup policy and consumer
# body, and that exact text identifies the program. With glslc the whole
# (consumer x policy) matrix and every policy builder is also compiled to
# SPIR-V; libshaderc compiles any other composition at runtime.
SIXEL_VULKAN_BUILDER_PREFIX = $(srcdir)/gpu-lookup-builder-prefix.glsl
SIXEL_VULKAN_POLICIES = $(srcdir)/lookup-policy-none-gpu.glsl \
    $(srcdir)/lookup-policy-fhedt-gpu.glsl \
    $(srcdir)/lookup-policy-bitlut-gpu.glsl \
    $(srcdir)/lookup-policy-eytzinger-gpu.glsl \
    $(srcdir)/lookup-policy-ordered-gpu.glsl \
    $(srcdir)/lookup-policy-vptree-gpu.glsl \
    $(srcdir)/lookup-policy-rbc-gpu.glsl \
    $(srcdir)/lookup-policy-mahalanobis-gpu.glsl
SIXEL_VULKAN_CONSUMER_PAIRS = $(srcdir)/gpu-palette-fused-prefix.glsl:$(srcdir)/gpu-palette-fused-suffix.glsl \
    $(srcdir)/dither-policy-lso2-gpu-prefix.glsl:$(srcdir)/dither-policy-lso2-gpu.glsl \
    $(srcdir)/dither-policy-fs-gpu-prefix.glsl:$(srcdir)/dither-policy-fs-gpu.glsl \
    $(srcdir)/dither-policy-atkinson-gpu-prefix.glsl:$(srcdir)/dither-policy-atkinson-gpu.glsl \
    $(srcdir)/dither-policy-jajuni-gpu-prefix.glsl:$(srcdir)/dither-policy-jajuni-gpu.glsl \
    $(srcdir)/dither-policy-stucki-gpu-prefix.glsl:$(srcdir)/dither-policy-stucki-gpu.glsl \
    $(srcdir)/dither-policy-burkes-gpu-prefix.glsl:$(srcdir)/dither-policy-burkes-gpu.glsl \
    $(srcdir)/dither-policy-sierra1-gpu-prefix.glsl:$(srcdir)/dither-policy-sierra1-gpu.glsl \
    $(srcdir)/dither-policy-sierra2-gpu-prefix.glsl:$(srcdir)/dither-policy-sierra2-gpu.glsl \
    $(srcdir)/dither-policy-sierra3-gpu-prefix.glsl:$(srcdir)/dither-policy-sierra3-gpu.glsl
SIXEL_VULKAN_INPUTS = $(srcdir)/gpu-lookup-builder-prefix.glsl \
    $(srcdir)/gpu-palette-fused-prefix.glsl \
    $(srcdir)/gpu-palette-fused-suffix.glsl \
    $(srcdir)/dither-policy-lso2-gpu-prefix.glsl \
    $(srcdir)/dither-policy-lso2-gpu.glsl \
    $(srcdir)/dither-policy-fs-gpu-prefix.glsl \
    $(srcdir)/dither-policy-fs-gpu.glsl \
    $(srcdir)/dither-policy-atkinson-gpu-prefix.glsl \
    $(srcdir)/dither-policy-atkinson-gpu.glsl \
    $(srcdir)/dither-policy-jajuni-gpu-prefix.glsl \
    $(srcdir)/dither-policy-jajuni-gpu.glsl \
    $(srcdir)/dither-policy-stucki-gpu-prefix.glsl \
    $(srcdir)/dither-policy-stucki-gpu.glsl \
    $(srcdir)/dither-policy-burkes-gpu-prefix.glsl \
    $(srcdir)/dither-policy-burkes-gpu.glsl \
    $(srcdir)/dither-policy-sierra1-gpu-prefix.glsl \
    $(srcdir)/dither-policy-sierra1-gpu.glsl \
    $(srcdir)/dither-policy-sierra2-gpu-prefix.glsl \
    $(srcdir)/dither-policy-sierra2-gpu.glsl \
    $(srcdir)/dither-policy-sierra3-gpu-prefix.glsl \
    $(srcdir)/dither-policy-sierra3-gpu.glsl \
    $(srcdir)/lookup-policy-none-gpu.glsl \
    $(srcdir)/lookup-policy-fhedt-gpu.glsl \
    $(srcdir)/lookup-policy-bitlut-gpu.glsl \
    $(srcdir)/lookup-policy-eytzinger-gpu.glsl \
    $(srcdir)/lookup-policy-ordered-gpu.glsl \
    $(srcdir)/lookup-policy-vptree-gpu.glsl \
    $(srcdir)/lookup-policy-rbc-gpu.glsl \
    $(srcdir)/lookup-policy-mahalanobis-gpu.glsl
SIXEL_VULKAN_FRAGMENTS =
if HAVE_VULKAN
SIXEL_VULKAN_FRAGMENTS += \
    $(patsubst $(srcdir)/%.glsl,generated-vulkan/%-shader.inc.h,$(SIXEL_VULKAN_INPUTS))
if HAVE_VULKAN_PREBUILT_SPIRV
SIXEL_VULKAN_FRAGMENTS += generated-vulkan/lookup-programs-spirv.inc.h
endif
endif
BUILT_SOURCES = $(SIXEL_METAL_FRAGMENTS) $(SIXEL_VULKAN_FRAGMENTS)
nodist_libsixel_la_SOURCES = $(SIXEL_METAL_FRAGMENTS) $(SIXEL_VULKAN_FRAGMENTS)

generated-vulkan/%-shader.inc.h: $(srcdir)/%.glsl \
        $(top_srcdir)/tools/embed-metal.sh $(top_srcdir)/tools/embed-metal.awk
	$(AM_V_GEN)AWK="$(AWK)" $(SHELL) $(top_srcdir)/tools/embed-metal.sh "$<" "$@"

generated-vulkan/lookup-programs-spirv.inc.h: $(SIXEL_VULKAN_INPUTS) \
        $(top_srcdir)/tools/gen-vulkan-programs.sh
	$(AM_V_GEN)$(SHELL) $(top_srcdir)/tools/gen-vulkan-programs.sh \
	    "$(GLSLC)" "$@" $(SIXEL_VULKAN_BUILDER_PREFIX) \
	    $(SIXEL_VULKAN_CONSUMER_PAIRS) -- $(SIXEL_VULKAN_POLICIES)

# Like the Metal consumers below, the phased build compiles objects
# directly; name the consumers so their embedded shaders exist first. A
# variable keeps automake from treating this as an override of its own
# compile rule.
SIXEL_VULKAN_CONSUMERS = libsixel_la-gpu-runtime-vulkan.lo \
    libsixel_la-gpu-palette-vulkan.lo \
    libsixel_la-dither-policy-lso2-gpu-vulkan.lo \
    libsixel_la-dither-policy-fs-gpu-vulkan.lo \
    libsixel_la-dither-policy-atkinson-gpu-vulkan.lo \
    libsixel_la-dither-policy-jajuni-gpu-vulkan.lo \
    libsixel_la-dither-policy-stucki-gpu-vulkan.lo \
    libsixel_la-dither-policy-burkes-gpu-vulkan.lo \
    libsixel_la-dither-policy-sierra1-gpu-vulkan.lo \
    libsixel_la-dither-policy-sierra2-gpu-vulkan.lo \
    libsixel_la-dither-policy-sierra3-gpu-vulkan.lo \
    libsixel_la-lookup-policy-none-gpu.lo \
    libsixel_la-lookup-policy-fhedt-gpu.lo \
    libsixel_la-lookup-policy-bitlut-gpu.lo \
    libsixel_la-lookup-policy-eytzinger-gpu.lo \
    libsixel_la-lookup-policy-ordered-gpu.lo \
    libsixel_la-lookup-policy-vptree-gpu.lo \
    libsixel_la-lookup-policy-rbc-gpu.lo \
    libsixel_la-lookup-policy-mahalanobis-gpu.lo
$(SIXEL_VULKAN_CONSUMERS): $(SIXEL_VULKAN_FRAGMENTS)

generated-metal/%-shader.inc.h: $(srcdir)/%.metal \
        $(top_srcdir)/tools/embed-metal.sh $(top_srcdir)/tools/embed-metal.awk
	$(AM_V_GEN)AWK="$(AWK)" $(SHELL) $(top_srcdir)/tools/embed-metal.sh "$<" "$@"

# The phased top-level build requests objects directly, bypassing all and
# BUILT_SOURCES. Declare each actual consumer as well as the all prerequisite.
SIXEL_METAL_CONSUMERS = \
    libsixel_la-dither-policy-lso2-gpu-metal.lo \
    libsixel_la-dither-policy-fs-gpu-metal.lo \
    libsixel_la-dither-policy-atkinson-gpu-metal.lo \
    libsixel_la-dither-policy-jajuni-gpu-metal.lo \
    libsixel_la-dither-policy-stucki-gpu-metal.lo \
    libsixel_la-dither-policy-burkes-gpu-metal.lo \
    libsixel_la-dither-policy-sierra1-gpu-metal.lo \
    libsixel_la-dither-policy-sierra2-gpu-metal.lo \
    libsixel_la-dither-policy-sierra3-gpu-metal.lo \
    libsixel_la-retained-plane-metal.lo \
    libsixel_la-lookup-policy-ordered-gpu.lo \
    libsixel_la-lookup-policy-none-gpu.lo \
    libsixel_la-lookup-policy-eytzinger-gpu.lo \
    libsixel_la-lookup-policy-gridlut-gpu.lo \
    libsixel_la-lookup-policy-blockcull-gpu.lo \
    libsixel_la-lookup-policy-bitlut-gpu.lo \
    libsixel_la-lookup-policy-fhedt-gpu.lo \
    libsixel_la-lookup-policy-delaunay-gpu.lo \
    libsixel_la-lookup-policy-vptree-gpu.lo \
    libsixel_la-lookup-policy-rbc-gpu.lo \
    libsixel_la-lookup-policy-mahalanobis-gpu.lo \
    libsixel_la-lookup-policy-certlut-gpu.lo \
    libsixel_la-gpu-palette-metal.lo
$(SIXEL_METAL_CONSUMERS): $(SIXEL_METAL_FRAGMENTS)

if WANT_AMALGAMATED_LIB
AMALGAMATION_SIXEL = $(top_builddir)/amalgamation/sixel.c
AMALGAMATION_TARGET = sixel.c
if AMALGAMATION_NEEDS_OBJC
AMALGAMATION_COMPILE = $(LTOBJCCOMPILE)
AMALGAMATION_CPPFLAGS = -x objective-c
else
AMALGAMATION_COMPILE = $(LTCOMPILE)
AMALGAMATION_CPPFLAGS =
endif
# Compile the large dither unit as a separate translation unit for
# libsixel amalgamation to avoid compiler ICEs on very large sources.
AMALGAMATION_CPPFLAGS += \
        -DSIXEL_AMALGAMATION_SPLIT_DITHER=1
BUILT_SOURCES += $(AMALGAMATION_SIXEL)
nodist_libsixel_la_SOURCES += $(AMALGAMATION_SIXEL)
endif

if COND_GCOV
MAYBE_COVERAGE = --coverage
endif
if CFLAGS_HAVE_COVERAGE
MAYBE_COVERAGE = --coverage
endif
if USING_CL
# Force cl/clang-cl driver mode to DLL when libtool links shared libraries.
# This keeps linker entrypoint selection consistent even with extra link
# flags such as coverage instrumentation.
#
# Pass '-LD' via libtool's compiler-driver escape hatch so it is not
# misparsed as '-L<path>' during libtool argument scanning.
MAYBE_MSVC_DLL_DRIVER_FLAG = -XCClinker -LD
endif

CLEANFILES = $(SIXEL_METAL_FRAGMENTS) $(SIXEL_VULKAN_FRAGMENTS) \
    *.gcno *.gcda *.gcov *.log test-output.*
DISTCLEANFILES = \
                $(DEPDIR)/libsixel_la-gpu-palette.Plo \
                $(DEPDIR)/libsixel_la-gpu-palette-metal.Plo

lib_LTLIBRARIES = libsixel.la

if WANT_AMALGAMATED_LIB
libsixel_la_SOURCES =
libsixel_la_SOURCES += \
                $(srcdir)/dither.c \
                $(srcdir)/dither.h \
                $(srcdir)/oklch-lattice256.h

# Keep this recipe tab-indented; GNU make requires tabs for commands.
$(AMALGAMATION_SIXEL): $(SIXEL_METAL_FRAGMENTS) $(top_builddir)/amalgamation/Makefile
	$(AM_V_GEN)$(MAKE) -C $(top_builddir)/converters completion_embed.h
	$(AM_V_GEN)$(MAKE) -C $(top_builddir)/amalgamation $(AMALGAMATION_TARGET)

# Keep this recipe tab-indented; GNU make requires tabs for commands.
libsixel_la-sixel.lo: $(AMALGAMATION_SIXEL)
	$(AMALGAMATION_COMPILE) -c -o $@ $<

else
# Isolate arithmetic flags from every other translation unit. Link these
# objects directly: cosmoar cannot extract dual-architecture convenience
# archives into the separate object directories expected by cosmocc.
EXTRA_LTLIBRARIES = libfloatlookup.la
libfloatlookup_la_SOURCES = $(srcdir)/lookup-vptree-float32.c \
    $(srcdir)/lookup-delaunay-float32.c \
    $(srcdir)/lookup-blockcull-float32.c
libfloatlookup_la_CPPFLAGS = $(libsixel_la_CPPFLAGS)
libfloatlookup_la_CFLAGS = $(libsixel_la_CFLAGS) $(FLOAT_LOOKUP_CFLAGS)
FLOAT_LOOKUP_LIB = $(libfloatlookup_la_OBJECTS)
libsixel_la_SOURCES = \
                $(srcdir)/output.c \
                $(srcdir)/output.h \
                $(srcdir)/output-factory.h \
                $(srcdir)/encode-result.c \
                $(srcdir)/encode-result.h \
                $(srcdir)/retained-plane.c \
                $(srcdir)/retained-plane-gpu.c \
                $(srcdir)/retained-plane-gpu.h \
                $(srcdir)/gpu-runtime-metal.h \
                $(srcdir)/gpu-runtime-vulkan.h \
                $(srcdir)/gpu-palette-vulkan.h \
                $(srcdir)/retained-plane-rows.c \
                $(srcdir)/retained-plane-rows.h \
                $(srcdir)/retained-plane-workers.c \
                $(srcdir)/retained-plane-workers.h \
                $(srcdir)/palette-apply.c \
                $(srcdir)/sixel-writer.c \
                $(srcdir)/sixel-writer.h \
                $(srcdir)/encoder-core.c \
                $(srcdir)/encoder-core.h \
                $(srcdir)/encoder-core-private.h \
                $(srcdir)/encoder-core-packet.h \
                $(srcdir)/encoder-band-schedule.h \
                $(srcdir)/encoder-core-indexed.inc.h \
                $(srcdir)/encoder-core-session.inc.h \
                $(srcdir)/encoder-playback-deferred.inc.h \
                $(srcdir)/encoder-playback-independent.inc.h \
                $(srcdir)/playback-ready.h \
                $(srcdir)/timer.c \
                $(srcdir)/timer.h \
                $(srcdir)/sleep.c \
                $(srcdir)/sleep.h \
                $(srcdir)/fromsixel.c \
                $(srcdir)/fromsixel-aspect.c \
                $(srcdir)/encoder-core-encode.c \
                $(srcdir)/encoder-core-highcolor.c \
                $(srcdir)/encoder-core-highcolor.h \
                $(srcdir)/lookup-policy.c \
                $(srcdir)/lookup-policy-none.c \
                $(srcdir)/lookup-policy-mono-darkbg.c \
                $(srcdir)/lookup-policy-mono-lightbg.c \
                $(srcdir)/lookup-policy-certlut.c \
                $(srcdir)/lookup-policy-5bit.c \
                $(srcdir)/lookup-policy-6bit.c \
                $(srcdir)/lookup-policy-eytzinger.c \
                $(srcdir)/lookup-policy-fhedt.c \
                $(srcdir)/lookup-policy-vptree.c \
                $(srcdir)/lookup-policy-delaunay.c \
                $(srcdir)/lookup-policy-gridlut.c \
                $(srcdir)/lookup-policy-ordered.c \
                $(srcdir)/lookup-policy-blockcull.c \
                $(srcdir)/lookup-policy-rbc.c \
                $(srcdir)/lookup-policy-mahalanobis.c \
                $(srcdir)/factory.c \
                $(srcdir)/components.c \
                $(srcdir)/lookup-fhedt-8bit.c \
                $(srcdir)/lookup-fhedt-8bit.h \
                $(srcdir)/lookup-fhedt-float32.c \
                $(srcdir)/lookup-fhedt-float32.h \
                $(srcdir)/lookup-vptree-8bit.c \
                $(srcdir)/lookup-delaunay-8bit.c \
                $(srcdir)/lookup-delaunay-graph.c \
                $(srcdir)/lookup-vptree-8bit.h \
                $(srcdir)/lookup-delaunay-8bit.h \
                $(srcdir)/lookup-delaunay-graph.h \
                $(srcdir)/lookup-vptree-float32.h \
                $(srcdir)/lookup-delaunay-float32.h \
                $(srcdir)/lookup-blockcull-float32.h \
                $(srcdir)/lookup-policy.h \
                $(srcdir)/lookup-policy-none.h \
                $(srcdir)/lookup-policy-mono-darkbg.h \
                $(srcdir)/lookup-policy-mono-lightbg.h \
                $(srcdir)/lookup-policy-certlut.h \
                $(srcdir)/lookup-policy-5bit.h \
                $(srcdir)/lookup-policy-6bit.h \
                $(srcdir)/lookup-policy-eytzinger.h \
                $(srcdir)/lookup-policy-fhedt.h \
                $(srcdir)/lookup-policy-vptree.h \
                $(srcdir)/lookup-policy-delaunay.h \
                $(srcdir)/lookup-policy-gridlut.h \
                $(srcdir)/lookup-policy-ordered.h \
                $(srcdir)/lookup-policy-blockcull.h \
                $(srcdir)/lookup-policy-rbc.h \
                $(srcdir)/lookup-policy-mahalanobis.h \
                $(srcdir)/factory.h \
                $(srcdir)/palette-heckbert.c \
                $(srcdir)/palette-heckbert.h \
                $(srcdir)/palette-common-cover.c \
                $(srcdir)/palette-cover-native.c \
                $(srcdir)/palette-common-cover.h \
                $(srcdir)/palette-common-merge.c \
                $(srcdir)/palette-common-merge.h \
                $(srcdir)/palette-common-snap.c \
                $(srcdir)/palette-snap-grid.c \
                $(srcdir)/palette-common-snap.h \
                $(srcdir)/palette-snap-grid.h \
                $(srcdir)/palette-kmeans.c \
                $(srcdir)/palette-kmeans-assign.c \
                $(srcdir)/palette-kmeans-assign.h \
                $(srcdir)/palette-kmeans-build.c \
                $(srcdir)/palette-kmeans-build.h \
                $(srcdir)/palette-kmeans-gpu.c \
                $(srcdir)/palette-kmeans-gpu.h \
                $(srcdir)/palette-kmeans.h \
                $(srcdir)/palette-bin-histogram.c \
                $(srcdir)/palette-bin-histogram.h \
                $(srcdir)/palette-plan.c \
                $(srcdir)/palette-plan.h \
                $(srcdir)/sample-statistics.c \
                $(srcdir)/sample-statistics.h \
                $(srcdir)/palette-sample.c \
                $(srcdir)/palette-sample.h \
                $(srcdir)/filter-palette-colors.c \
                $(srcdir)/filter-palette-colors.h \
                $(srcdir)/filter-factory-palette-colors.c \
                $(srcdir)/filter-factory-palette-colors.h \
                $(srcdir)/sample-stream.c \
                $(srcdir)/sample-stream.h \
                $(srcdir)/weighted-point-set.c \
                $(srcdir)/weighted-point-set.h \
                $(srcdir)/progressive-palette.c \
                $(srcdir)/progressive-select.c \
                $(srcdir)/progressive-apply.c \
                $(srcdir)/progressive-palette.h \
                $(srcdir)/progressive-select.h \
                $(srcdir)/progressive-apply.h \
                $(srcdir)/progressive.c \
                $(srcdir)/progressive-video.c \
                $(srcdir)/progressive-measure.c \
                $(srcdir)/progressive-measure.h \
                $(srcdir)/progressive-predict.c \
                $(srcdir)/progressive-predict.h \
                $(srcdir)/progressive-ladder.c \
                $(srcdir)/progressive-ladder.h \
                $(srcdir)/presentation.c \
                $(srcdir)/presentation.h \
                $(srcdir)/progressive.h \
                $(srcdir)/palette-kmedoids.c \
                $(srcdir)/palette-kmedoids.h \
                $(srcdir)/palette-kcenter.c \
                $(srcdir)/palette-kcenter.h \
                $(srcdir)/palette.c \
                $(srcdir)/palette-factory.h \
                $(srcdir)/palette.h \
                $(srcdir)/palette-private.h \
                $(srcdir)/dither.c \
                $(srcdir)/dither.h \
                $(srcdir)/oklch-lattice256.h \
                $(srcdir)/dither-policy.c \
                $(srcdir)/dither-policy-contract.c \
                $(srcdir)/dither-policy-private.h \
                $(srcdir)/dither-policy.h \
                $(srcdir)/dither-policy-none.c \
                $(srcdir)/dither-policy-fs.c \
                $(srcdir)/dither-policy-atkinson.c \
                $(srcdir)/dither-policy-jajuni.c \
                $(srcdir)/dither-policy-stucki.c \
                $(srcdir)/dither-policy-burkes.c \
                $(srcdir)/dither-policy-sierra1.c \
                $(srcdir)/dither-policy-sierra2.c \
                $(srcdir)/dither-policy-sierra3.c \
                $(srcdir)/dither-policy-a-dither.c \
                $(srcdir)/dither-policy-x-dither.c \
                $(srcdir)/dither-policy-bluenoise.c \
                $(srcdir)/dither-policy-lso2-gpu.c \
                $(srcdir)/dither-policy-fs-gpu.c \
                $(srcdir)/dither-policy-atkinson-gpu.c \
                $(srcdir)/dither-policy-jajuni-gpu.c \
                $(srcdir)/dither-policy-stucki-gpu.c \
                $(srcdir)/dither-policy-burkes-gpu.c \
                $(srcdir)/dither-policy-sierra1-gpu.c \
                $(srcdir)/dither-policy-sierra2-gpu.c \
                $(srcdir)/dither-policy-sierra3-gpu.c \
                $(srcdir)/lso2-gpu-coefficients.h \
                $(srcdir)/dither-policy-lso2.c \
                $(srcdir)/dither-policy-none.h \
                $(srcdir)/dither-policy-fs.h \
                $(srcdir)/dither-policy-atkinson.h \
                $(srcdir)/dither-policy-jajuni.h \
                $(srcdir)/dither-policy-stucki.h \
                $(srcdir)/dither-policy-burkes.h \
                $(srcdir)/dither-policy-sierra1.h \
                $(srcdir)/dither-policy-sierra2.h \
                $(srcdir)/dither-policy-sierra3.h \
                $(srcdir)/dither-policy-lso2-gpu.h \
                $(srcdir)/dither-policy-fs-gpu.h \
                $(srcdir)/dither-policy-atkinson-gpu.h \
                $(srcdir)/dither-policy-jajuni-gpu.h \
                $(srcdir)/dither-policy-stucki-gpu.h \
                $(srcdir)/dither-policy-burkes-gpu.h \
                $(srcdir)/dither-policy-sierra1-gpu.h \
                $(srcdir)/dither-policy-sierra2-gpu.h \
                $(srcdir)/dither-policy-sierra3-gpu.h \
    $(srcdir)/dither-policy-lso2.h \
                $(srcdir)/dither-policy-a-dither.h \
                $(srcdir)/dither-policy-x-dither.h \
                $(srcdir)/dither-policy-bluenoise.h \
                $(srcdir)/dither-clamp.c \
                $(srcdir)/dither-carry.c \
                $(srcdir)/dither-clamp.h \
                $(srcdir)/dither-carry.h \
                $(srcdir)/dither-common-pipeline.c \
                $(srcdir)/dither-common-pipeline.h \
                $(srcdir)/gpu-dequant.c \
                $(srcdir)/gpu-dequant.h \
                $(srcdir)/delta-residual.c \
                $(srcdir)/delta-residual.h \
                $(srcdir)/gpu-palette.c \
                $(srcdir)/gpu-statistics.c \
                $(srcdir)/lookup-policy-ordered-gpu.c \
                $(srcdir)/lookup-policy-none-gpu.c \
                $(srcdir)/lookup-policy-eytzinger-gpu.c \
                $(srcdir)/lookup-policy-gridlut-gpu.c \
                $(srcdir)/lookup-policy-blockcull-gpu.c \
                $(srcdir)/lookup-policy-bitlut-gpu.c \
                $(srcdir)/lookup-policy-fhedt-gpu.c \
                $(srcdir)/lookup-policy-delaunay-gpu.c \
                $(srcdir)/lookup-policy-vptree-gpu.c \
                $(srcdir)/lookup-policy-rbc-gpu.c \
                $(srcdir)/lookup-policy-mahalanobis-gpu.c \
                $(srcdir)/lookup-policy-certlut-gpu.c \
                $(srcdir)/lookup-policy-gpu-legacy.c \
                $(srcdir)/lookup-policy-gpu-registry.c \
                $(srcdir)/lookup-policy-gpu-compat.c \
                $(srcdir)/lookup-policy-gpu.h \
                $(srcdir)/lookup-policy-ordered-gpu.h \
                $(srcdir)/lookup-policy-none-gpu.h \
                $(srcdir)/lookup-policy-eytzinger-gpu.h \
                $(srcdir)/lookup-policy-gridlut-gpu.h \
                $(srcdir)/lookup-policy-blockcull-gpu.h \
                $(srcdir)/lookup-policy-bitlut-gpu.h \
                $(srcdir)/lookup-policy-fhedt-gpu.h \
                $(srcdir)/lookup-policy-delaunay-gpu.h \
                $(srcdir)/lookup-policy-vptree-gpu.h \
                $(srcdir)/lookup-policy-rbc-gpu.h \
                $(srcdir)/lookup-policy-mahalanobis-gpu.h \
                $(srcdir)/lookup-policy-certlut-gpu.h \
                $(srcdir)/lookup-policy-gpu-legacy.h \
                $(srcdir)/gpu-program.h \
                $(srcdir)/gpu-palette-program.inc.h \
                $(srcdir)/gpu-palette-fused-prefix.inc.h \
                $(srcdir)/gpu-palette-fused-suffix.inc.h \
                $(srcdir)/dither-lookup-preparer.inc.h \
                $(srcdir)/progressive-gpu.c \
                $(srcdir)/progressive-gpu.h \
                $(srcdir)/gpu-palette.h \
                $(srcdir)/gpu-statistics.h \
                $(srcdir)/bluenoise_64x64.h \
                $(srcdir)/timeline-logger.c \
                $(srcdir)/timeline-logger.h \
                $(srcdir)/timeline-writer.c \
                $(srcdir)/timeline-writer.h \
                $(srcdir)/colorspace.c \
                $(srcdir)/colorspace-hls.c \
                $(srcdir)/colorspace.h \
                $(srcdir)/frame.c \
                $(srcdir)/frame.h \
                $(srcdir)/frame-factory.h \
                $(srcdir)/frame-private.h \
                $(srcdir)/frame-normalize.h \
                $(srcdir)/cpu.c \
                $(srcdir)/cpu.h \
                $(srcdir)/pixelformat.c \
                $(srcdir)/pixelformat.h \
                $(srcdir)/filter-colors.c \
                $(srcdir)/filter-colors.h \
                $(srcdir)/filter-gamut.c \
                $(srcdir)/filter-gamut.h \
                $(srcdir)/filter-pipeline.c \
                $(srcdir)/filter-pipeline.h \
                $(srcdir)/filter-load.c \
                $(srcdir)/filter-load.h \
                $(srcdir)/filter-factory.c \
                $(srcdir)/filter-factory.h \
                $(srcdir)/filter-factory-binning.c \
                $(srcdir)/filter-factory-binning.h \
                $(srcdir)/filter-final-merge.c \
                $(srcdir)/filter-final-merge.h \
                $(srcdir)/filter-dither.c \
                $(srcdir)/filter-dither.h \
                $(srcdir)/filter-gradient.c \
                $(srcdir)/filter-gradient.h \
                $(srcdir)/filter-encode.c \
                $(srcdir)/filter-encode.h \
                $(srcdir)/filter-lookup.c \
                $(srcdir)/filter-lookup.h \
                $(srcdir)/filter-binning.c \
                $(srcdir)/filter-binning.h \
                $(srcdir)/filter-palette.c \
                $(srcdir)/filter-palette.h \
                $(srcdir)/filter-fhedt.c \
                $(srcdir)/filter-fhedt.h \
                $(srcdir)/filter-vptree.c \
                $(srcdir)/filter-vptree.h \
                $(srcdir)/filter-eytzinger.c \
                $(srcdir)/filter-eytzinger.h \
                $(srcdir)/filter-clip.c \
                $(srcdir)/filter-clip.h \
                $(srcdir)/filter-resize.c \
                $(srcdir)/filter-resize.h \
                $(srcdir)/filter-sample.c \
                $(srcdir)/filter-sample.h \
                $(srcdir)/filter.c \
                $(srcdir)/filter.h \
                $(srcdir)/scale.c \
                $(srcdir)/scale.h \
                $(srcdir)/chunk.c \
                $(srcdir)/chunk.h \
                $(srcdir)/chunk-factory.h \
                $(srcdir)/chunk-view.h \
                $(srcdir)/gamut.c \
                $(srcdir)/gamut.h \
                $(srcdir)/color-option.c \
                $(srcdir)/color-option.h \
                $(srcdir)/cms.c \
                $(srcdir)/png-chrm-arithmetic.c \
                $(srcdir)/png-chrm-arithmetic.h \
                $(srcdir)/cms.h \
                $(srcdir)/named-color.h \
                $(srcdir)/cms-curv.h \
                $(srcdir)/icc-parse.c \
                $(srcdir)/icc-parse.h \
                $(srcdir)/icc-apply.c \
                $(srcdir)/icc-apply.h \
                $(srcdir)/loader.c \
                $(srcdir)/loader-gdk-pixbuf2.c \
                $(srcdir)/loader-gnome-thumbnailer.c \
                $(srcdir)/loader-coregraphics.c \
                $(srcdir)/loader-quicklook.c \
                $(srcdir)/loader-wic.c \
                $(srcdir)/loader-gd.c \
                $(srcdir)/loader-libpng.c \
                $(srcdir)/loader-libtiff.c \
                $(srcdir)/loader-librsvg.c \
                $(srcdir)/loader-libjpeg.c \
                $(srcdir)/loader-libwebp.c \
                $(srcdir)/loader-builtin.c \
                $(srcdir)/loader-common.c \
                $(srcdir)/loader-order-schema.c \
                $(srcdir)/loader-manager.c \
                $(srcdir)/loader.h \
                $(srcdir)/loader-builtin.h \
                $(srcdir)/loader-common.h \
                $(srcdir)/loader-order-schema.h \
                $(srcdir)/loader-manager.h \
                $(srcdir)/loader-coregraphics.h \
                $(srcdir)/loader-gd.h \
                $(srcdir)/loader-gdk-pixbuf2.h \
                $(srcdir)/loader-gnome-thumbnailer.h \
                $(srcdir)/loader-libjpeg.h \
                $(srcdir)/loader-libpng.h \
                $(srcdir)/loader-libpng-chrm.h \
                $(srcdir)/loader-libpng-chrm-area.h \
                $(srcdir)/loader-libtiff.h \
                $(srcdir)/loader-librsvg.h \
                $(srcdir)/loader-libwebp.h \
                $(srcdir)/loader-quicklook.h \
                $(srcdir)/loader-wic.h \
                $(srcdir)/frompnm.c \
                $(srcdir)/frompnm.h \
                $(srcdir)/frompng.c \
                $(srcdir)/frompng.h \
                $(srcdir)/fromhdr.c \
                $(srcdir)/fromhdr.h \
                $(srcdir)/fromtga.c \
                $(srcdir)/fromtga.h \
                $(srcdir)/frompsd.c \
                $(srcdir)/frompsd.h \
                $(srcdir)/frompsd-internal.h \
                $(srcdir)/frompsd-trace.c \
                $(srcdir)/frompsd-header.c \
                $(srcdir)/frompsd-color.c \
                $(srcdir)/frompsd-color.h \
                $(srcdir)/loader-psd-color.c \
                $(srcdir)/loader-psd-color.h \
                $(srcdir)/loader-psd-process.c \
                $(srcdir)/loader-psd-process.h \
                $(srcdir)/fromgif.c \
                $(srcdir)/fromgif.h \
                $(srcdir)/frombmp-parser.c \
                $(srcdir)/frombmp.c \
                $(srcdir)/frombmp.h \
                $(srcdir)/frombmp-internal.h \
                $(srcdir)/clipboard.c \
                $(srcdir)/clipboard.h \
                $(srcdir)/encoder.c \
                $(srcdir)/encoder.h \
                $(srcdir)/encoder-palette.h \
                $(srcdir)/encoder-palette-store.c \
                $(srcdir)/encoder-palette-scheduler.c \
                $(srcdir)/encoder-palette-auto.c \
                $(srcdir)/encoder-palette-store.h \
                $(srcdir)/encoder-palette-scheduler.h \
                $(srcdir)/encoder-palette-auto.h \
                $(srcdir)/mapfile.c \
                $(srcdir)/mapfile.h \
                $(srcdir)/drcs.c \
                $(srcdir)/drcs.h \
                $(srcdir)/planner.c \
                $(srcdir)/planner.h \
                $(srcdir)/decoder.c \
                $(srcdir)/decoder.h \
                $(srcdir)/decoder-parallel.c \
                $(srcdir)/decoder-parallel.h \
                $(srcdir)/decoder-image.h \
                $(srcdir)/fromsixel-internal.h \
                $(srcdir)/decoder-ormode.h \
                $(srcdir)/decoder-columns.h \
                $(srcdir)/decoder-simd.h \
                $(srcdir)/options.c \
                $(srcdir)/options.h \
                $(srcdir)/options-registry.c \
                $(srcdir)/options-registry.h \
                $(srcdir)/path.c \
                $(srcdir)/path.h \
                $(srcdir)/stb_image.h \
                $(srcdir)/writer.c \
                $(srcdir)/writer.h \
                $(srcdir)/stb_image_write.c \
                $(srcdir)/stb_image_write.h \
                $(srcdir)/status.c \
                $(srcdir)/status.h \
                $(srcdir)/stdio_stub.h \
                $(srcdir)/compat_stub.c \
                $(srcdir)/compat_stub.h \
                $(srcdir)/malloc_stub.c \
                $(srcdir)/malloc_stub.h \
                $(srcdir)/allocator.c \
                $(srcdir)/allocator.h \
                $(srcdir)/tty.c \
                $(srcdir)/tty.h \
                $(srcdir)/threading.c \
                $(srcdir)/threading.h \
                $(srcdir)/pthread-once.h \
                $(srcdir)/threadpool.c \
                $(srcdir)/threadpool.h \
                $(srcdir)/probe.c \
                $(srcdir)/probe.h \
                $(srcdir)/sixel_decode_pixels.c \
                $(srcdir)/sixel_decode_pixels.h \
                $(srcdir)/sixel_atomic_fallback.c \
                $(srcdir)/sixel_atomic.h \
                $(srcdir)/classid-factory.h \
                $(srcdir)/classid-service.h \
                $(srcdir)/rgblookup.h \
                $(srcdir)/lso2.h
if HAVE_QUICKLOOK_THUMBNAILING
libsixel_la_SOURCES += \
$(srcdir)/quicklook_thumbnailing.m
endif
if HAVE_VULKAN
libsixel_la_SOURCES += \
$(srcdir)/gpu-runtime-vulkan.c \
$(srcdir)/gpu-palette-vulkan.c \
$(srcdir)/dither-policy-lso2-gpu-vulkan.c \
$(srcdir)/dither-policy-fs-gpu-vulkan.c \
$(srcdir)/dither-policy-atkinson-gpu-vulkan.c \
$(srcdir)/dither-policy-jajuni-gpu-vulkan.c \
$(srcdir)/dither-policy-stucki-gpu-vulkan.c \
$(srcdir)/dither-policy-burkes-gpu-vulkan.c \
$(srcdir)/dither-policy-sierra1-gpu-vulkan.c \
$(srcdir)/dither-policy-sierra2-gpu-vulkan.c \
$(srcdir)/dither-policy-sierra3-gpu-vulkan.c
endif
if HAVE_METAL
libsixel_la_SOURCES += \
$(srcdir)/dither-policy-lso2-gpu-metal.m \
$(srcdir)/dither-policy-fs-gpu-metal.m \
$(srcdir)/dither-policy-atkinson-gpu-metal.m \
$(srcdir)/dither-policy-jajuni-gpu-metal.m \
$(srcdir)/dither-policy-stucki-gpu-metal.m \
$(srcdir)/dither-policy-burkes-gpu-metal.m \
$(srcdir)/dither-policy-sierra1-gpu-metal.m \
$(srcdir)/dither-policy-sierra2-gpu-metal.m \
$(srcdir)/dither-policy-sierra3-gpu-metal.m \
$(srcdir)/gpu-palette-metal.m \
$(srcdir)/retained-plane-metal.m \
$(srcdir)/lookup-policy-fhedt-gpu.inc.h \
$(srcdir)/lookup-policy-bitlut-gpu.inc.h \
$(srcdir)/palette-kmeans-metal.m \
$(srcdir)/progressive-gpu-metal.m
endif
if HAVE_CLIPBOARD_MACOS_APPKIT
libsixel_la_SOURCES += \
$(srcdir)/clipboard_macos.m
endif
if HAVE_CLIPBOARD_MACOS_CARBON
libsixel_la_SOURCES += \
$(srcdir)/clipboard_carbon.c
endif
endif
libsixel_la_CPPFLAGS = -I$(top_builddir)/include/ -I$(top_srcdir)/include \
                -I$(top_srcdir) $(AM_CPPFLAGS) \
                @THREAD_CPPFLAGS@ $(AMALGAMATION_CPPFLAGS)
libsixel_la_CFLAGS = $(CFLAGS) $(AM_CFLAGS) $(MAYBE_COVERAGE) \
                $(LIBCURL_CFLAGS) \
                $(LIBFETCH_CFLAGS) \
                $(WINHTTP_CFLAGS) \
                $(LIBPNG_CFLAGS) \
                $(LIBTIFF_CFLAGS) \
                $(LIBJPEG_CFLAGS) \
                $(LCMS2_CFLAGS) \
                $(LIBWEBP_CFLAGS) \
                $(LIBRSVG_CFLAGS) \
                $(GDK_PIXBUF_CFLAGS) \
                $(GD_CFLAGS) \
                $(WIC_CFLAGS) \
                $(COREGRAPHICS_CFLAGS) \
                $(QUICKLOOK_CFLAGS) \
                $(METAL_CFLAGS) \
                $(VULKAN_CFLAGS) \
                $(SHADERC_CFLAGS) \
                $(CLIPBOARD_CFLAGS) \
                @THREAD_CFLAGS@
if BUILDING_ON_WINDOWS
libsixel_la_LDFLAGS = -no-undefined -version-info $(LS_LTVERSION) \
                $(MAYBE_MSVC_DLL_DRIVER_FLAG) $(MAYBE_COVERAGE) \
                $(LIBSIXEL_LDFLAGS)
else
libsixel_la_LDFLAGS = -version-info $(LS_LTVERSION) \
                $(MAYBE_COVERAGE) $(LIBSIXEL_LDFLAGS)
endif
LS_LT_CURRENT = $(word 1,$(subst :, ,$(LS_LTVERSION)))
libsixel_la_LIBADD = $(FLOAT_LOOKUP_LIB) -lm \
                $(LIBCURL_LIBS) \
                $(LIBFETCH_LIBS) \
                $(WINHTTP_LIBS) \
                $(LIBPNG_LIBS) \
                $(LIBTIFF_LIBS) \
                $(LIBJPEG_LIBS) \
                $(LCMS2_LIBS) \
                $(LIBWEBP_LIBS) \
                $(LIBRSVG_LIBS) \
                $(GDK_PIXBUF_LIBS) \
                $(GD_LIBS) \
                $(WIC_LIBS) \
                $(COREGRAPHICS_LIBS) \
                $(QUICKLOOK_LIBS) \
                $(METAL_LIBS) \
                $(VULKAN_LIBS) \
                $(SHADERC_LIBS) \
                $(CLIPBOARD_LIBS) \
                @THREAD_LIBS@

dist_man_MANS = $(srcdir)/sixel.5
EXTRA_DIST = $(srcdir)/rgblookup.gperf \
             $(srcdir)/classid-factory.gperf \
             $(srcdir)/classid-service.gperf

.PHONY: libobjs libsixel-import-lib-alias

# Build only the libtool object set for libsixel.
#
# This target lets the top-level Makefile overlap executable object
# compilation in other directories with the libsixel link step.
if OPENVMS_GNV
# GNV make can report the following libtool object target as failed after a
# successful compile when many libtool objects are prerequisites of one target.
# Invoke one recursive make per object so each compile has a fresh make state.
libobjs:
	@set -e; \
	for obj in $(libsixel_la_OBJECTS); do \
		$(MAKE) $(AM_MAKEFLAGS) "$$obj" || exit $$?; \
	done
else
libobjs: $(libsixel_la_OBJECTS)
endif

if BUILDING_ON_WINDOWS
if USING_CL
# clang-cl/link.exe may emit only versioned import libraries such as
# '.libs/sixel-1.lib'. Provide the stable '.libs/sixel.dll.lib' alias
# explicitly so dependent program links can always resolve it.
$(top_builddir)/src/.libs/sixel.dll.lib: libsixel.la
	@if test -f .libs/sixel.dll.lib; then \
		:; \
	elif test -f .libs/sixel-$(LS_LT_CURRENT).lib; then \
		cp .libs/sixel-$(LS_LT_CURRENT).lib .libs/sixel.dll.lib; \
	elif test -f .libs/sixel.lib; then \
		cp .libs/sixel.lib .libs/sixel.dll.lib; \
	fi
LIBSIXEL_IMPORT_LIB_ALIAS_DEPS = $(top_builddir)/src/.libs/sixel.dll.lib
endif
endif

# Keep this helper target always defined so top-level phase orchestration can
# call it unconditionally. It becomes a no-op on non-MSVC builds.
libsixel-import-lib-alias: $(LIBSIXEL_IMPORT_LIB_ALIAS_DEPS)

GPERF_OPT=--ignore-case --language=ANSI-C
GPERF_FACTORY_CLASSID_OPT=--language=ANSI-C \
                  -m5 \
                  -t \
                  -H sixel_factory_classid_hash \
                  -W sixel_factory_classid_wordlist
GPERF_SERVICEID_OPT=--language=ANSI-C \
                  -k1 \
                  -H sixel_components_serviceid_hash \
                  -W sixel_components_serviceid_wordlist

LOOKUP_POLICY_CLASSID_INPUTS = \
    $(srcdir)/lookup-policy-5bit.h \
    $(srcdir)/lookup-policy-6bit.h \
    $(srcdir)/lookup-policy-certlut.h \
    $(srcdir)/lookup-policy-eytzinger.h \
    $(srcdir)/lookup-policy-fhedt.h \
    $(srcdir)/lookup-policy-mahalanobis.h \
    $(srcdir)/lookup-policy-mono-darkbg.h \
    $(srcdir)/lookup-policy-mono-lightbg.h \
    $(srcdir)/lookup-policy-none.h \
    $(srcdir)/lookup-policy-rbc.h \
    $(srcdir)/lookup-policy-vptree.h \
    $(srcdir)/lookup-policy-delaunay.h \
    $(srcdir)/lookup-policy-gridlut.h \
    $(srcdir)/lookup-policy-ordered.h \
    $(srcdir)/lookup-policy-blockcull.h

DITHER_POLICY_CLASSID_INPUTS = \
    $(srcdir)/dither-policy-none.h \
    $(srcdir)/dither-policy-fs.h \
    $(srcdir)/dither-policy-atkinson.h \
    $(srcdir)/dither-policy-jajuni.h \
    $(srcdir)/dither-policy-stucki.h \
    $(srcdir)/dither-policy-burkes.h \
    $(srcdir)/dither-policy-sierra1.h \
    $(srcdir)/dither-policy-sierra2.h \
    $(srcdir)/dither-policy-sierra3.h \
    $(srcdir)/dither-policy-lso2-gpu.h \
    $(srcdir)/dither-policy-fs-gpu.h \
    $(srcdir)/dither-policy-atkinson-gpu.h \
    $(srcdir)/dither-policy-jajuni-gpu.h \
    $(srcdir)/dither-policy-stucki-gpu.h \
    $(srcdir)/dither-policy-burkes-gpu.h \
    $(srcdir)/dither-policy-sierra1-gpu.h \
    $(srcdir)/dither-policy-sierra2-gpu.h \
    $(srcdir)/dither-policy-sierra3-gpu.h \
    $(srcdir)/dither-policy-lso2.h \
    $(srcdir)/dither-policy-a-dither.h \
    $(srcdir)/dither-policy-x-dither.h \
    $(srcdir)/dither-policy-bluenoise.h

LOADER_COMPONENT_CLASSID_INPUTS = \
    $(srcdir)/loader-builtin.h \
    $(srcdir)/loader-coregraphics.h \
    $(srcdir)/loader-gd.h \
    $(srcdir)/loader-gdk-pixbuf2.h \
    $(srcdir)/loader-gnome-thumbnailer.h \
    $(srcdir)/loader-libjpeg.h \
    $(srcdir)/loader-libpng.h \
    $(srcdir)/loader-librsvg.h \
    $(srcdir)/loader-libtiff.h \
    $(srcdir)/loader-libwebp.h \
    $(srcdir)/loader-quicklook.h \
    $(srcdir)/loader-wic.h

FACTORY_CLASSID_INPUTS = \
    $(LOOKUP_POLICY_CLASSID_INPUTS) \
    $(DITHER_POLICY_CLASSID_INPUTS) \
    $(LOADER_COMPONENT_CLASSID_INPUTS) \
    $(srcdir)/encode-result.h \
    $(srcdir)/sixel-writer.h \
    $(srcdir)/encoder-core.h \
    $(srcdir)/chunk.h \
    $(srcdir)/frame.h \
    $(srcdir)/palette.h \
    $(srcdir)/loader-manager.h \
    $(srcdir)/timeline-logger.h

SERVICEID_CLASSID_INPUTS = \
    $(srcdir)/factory.h \
    $(srcdir)/timeline-writer.h \
    $(srcdir)/threadpool.h

# Attribute diagnostics and coverage to the distributed C headers. gperf's
# input #line paths depend on the generator's working directory.
# Keep this recipe tab-indented; GNU make requires tabs for commands.
gen-rgblookup: $(srcdir)/rgblookup.gperf
	gperf -C -N lookup_rgb $(GPERF_OPT) $<  |\
	sed -e '/^#line /d' -e 's/{""}/\{"", 0, 0, 0\}/g'          |\
	astyle                                  > $(srcdir)/rgblookup.h
	printf '%s\n' '#undef TOTAL_KEYWORDS'    \
	              '#undef MIN_WORD_LENGTH'   \
	              '#undef MAX_WORD_LENGTH'   \
	              '#undef MIN_HASH_VALUE'    \
	              '#undef MAX_HASH_VALUE'    \
	        >> $(srcdir)/rgblookup.h

# Keep this recipe tab-indented; GNU make requires tabs for commands.
gen-classid: $(FACTORY_CLASSID_INPUTS) $(SERVICEID_CLASSID_INPUTS)
	$(AWK) -f $(top_srcdir)/tools/gen_factory_classid_gperf.awk \
	        $(FACTORY_CLASSID_INPUTS) > $(srcdir)/classid-factory.gperf
	gperf -C -N sixel_factory_classid_lookup $(GPERF_FACTORY_CLASSID_OPT) \
	        $(srcdir)/classid-factory.gperf                              |\
	sed -e '/^#line /d' -e 's/{""}/\{"", 0\}/g'                   \
	        > $(srcdir)/classid-factory.h
	printf '%s\n' '#undef TOTAL_KEYWORDS'                                \
	              '#undef MIN_WORD_LENGTH'                               \
	              '#undef MAX_WORD_LENGTH'                               \
	              '#undef MIN_HASH_VALUE'                                \
	              '#undef MAX_HASH_VALUE'                                \
	        >> $(srcdir)/classid-factory.h
	$(AWK) -f $(top_srcdir)/tools/gen_serviceid_gperf.awk \
	        $(SERVICEID_CLASSID_INPUTS) > $(srcdir)/classid-service.gperf
	gperf -C -N sixel_components_serviceid_lookup $(GPERF_SERVICEID_OPT) \
	        $(srcdir)/classid-service.gperf                              |\
	sed -e '/^#line /d' -e 's/{""}/\{"", 0\}/g'                   \
	        > $(srcdir)/classid-service.h
	printf '%s\n' '#undef TOTAL_KEYWORDS'                                \
	              '#undef MIN_WORD_LENGTH'                               \
	              '#undef MAX_WORD_LENGTH'                               \
	              '#undef MIN_HASH_VALUE'                                \
	              '#undef MAX_HASH_VALUE'                                \
	        >> $(srcdir)/classid-service.h



if WITH_SHEBANG_FILE
SIXEL_SHEBANG_TARGETS = $(check_PROGRAMS)
SIXEL_SHEBANG_STAMP =

check-local: $(SIXEL_SHEBANG_STAMP)

# Keep this recipe tab-indented; GNU make requires tabs for commands.
$(SIXEL_SHEBANG_STAMP): $(SIXEL_SHEBANG_TARGETS)
	@if test -n "$(SIXEL_SHEBANG_TARGETS)"; then \
	        $(SIXEL_SHEBANG_HELPER) "$(SIXEL_SHEBANG_FILE)" \
	                $(SIXEL_SHEBANG_TARGETS); \
	fi
	@touch $@

CLEANFILES += $(SIXEL_SHEBANG_STAMP)
endif

include $(top_srcdir)/build-aux/dependency-bootstrap.am

EXTRA_DIST += lso2-gpu.key $(SIXEL_METAL_INPUTS) generated-metal/meson.build \
    $(SIXEL_VULKAN_INPUTS) generated-vulkan/meson.build
