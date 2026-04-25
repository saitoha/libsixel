
srcdir = @srcdir@
top_srcdir = @top_srcdir@
VPATH = @srcdir@
prefix = @prefix@
exec_prefix = @exec_prefix@
top_builddir = @top_builddir@
builddir = @builddir@

SIXEL_SHEBANG_FILE = @SIXEL_SHEBANG_FILE@
SIXEL_SHEBANG_HELPER = $(top_srcdir)/build-aux/sixel-apply-shebang.sh

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
# Compile large dither units as separate translation units for libsixel
# amalgamation to avoid compiler ICEs on very large sources.
AMALGAMATION_CPPFLAGS += \
        -DSIXEL_AMALGAMATION_SPLIT_DITHER=1 \
        -DSIXEL_AMALGAMATION_SPLIT_DITHER_FIXED_FLOAT32=1
BUILT_SOURCES = $(AMALGAMATION_SIXEL)
nodist_libsixel_la_SOURCES = $(AMALGAMATION_SIXEL)
endif

if COND_GCOV
MAYBE_COVERAGE = --coverage
endif
if CFLAGS_HAVE_COVERAGE
MAYBE_COVERAGE = --coverage
endif

CLEANFILES = *.gcno *.gcda *.gcov *.log test-output.*

lib_LTLIBRARIES = libsixel.la

if WANT_AMALGAMATED_LIB
libsixel_la_SOURCES =
libsixel_la_SOURCES += \
                $(srcdir)/dither.c \
                $(srcdir)/dither.h
libsixel_la_SOURCES += \
                $(srcdir)/dither-fixed-float32.c \
                $(srcdir)/dither-fixed-float32.h

# Keep this recipe tab-indented; GNU make requires tabs for commands.
$(AMALGAMATION_SIXEL): $(top_builddir)/amalgamation/Makefile
	$(AM_V_GEN)$(MAKE) -C $(top_builddir)/converters completion_embed.h
	$(AM_V_GEN)$(MAKE) -C $(top_builddir)/amalgamation $(AMALGAMATION_TARGET)

# Keep this recipe tab-indented; GNU make requires tabs for commands.
libsixel_la-sixel.lo: $(AMALGAMATION_SIXEL)
	$(AMALGAMATION_COMPILE) -c -o $@ $<

else
libsixel_la_SOURCES = \
                $(srcdir)/output.c \
                $(srcdir)/output.h \
                $(srcdir)/timer.c \
                $(srcdir)/timer.h \
                $(srcdir)/sleep.c \
                $(srcdir)/sleep.h \
                $(srcdir)/fromsixel.c \
                $(srcdir)/tosixel.c \
                $(srcdir)/tosixel-highcolor.c \
                $(srcdir)/tosixel-highcolor.h \
                $(srcdir)/lookup-policy.c \
                $(srcdir)/lookup-policy-normal.c \
                $(srcdir)/lookup-policy-mono-darkbg.c \
                $(srcdir)/lookup-policy-mono-lightbg.c \
                $(srcdir)/lookup-policy-certlut.c \
                $(srcdir)/lookup-policy-5bit.c \
                $(srcdir)/lookup-policy-6bit.c \
                $(srcdir)/lookup-policy-eytzinger.c \
                $(srcdir)/lookup-policy-fhedt.c \
                $(srcdir)/lookup-policy-vptree.c \
                $(srcdir)/lookup-policy-rbc.c \
                $(srcdir)/lookup-policy-mahalanobis.c \
                $(srcdir)/factory.c \
                $(srcdir)/components.c \
                $(srcdir)/lookup-fhedt-8bit.c \
                $(srcdir)/lookup-fhedt-8bit.h \
                $(srcdir)/lookup-fhedt-float32.c \
                $(srcdir)/lookup-fhedt-float32.h \
                $(srcdir)/lookup-vptree-8bit.c \
                $(srcdir)/lookup-vptree-8bit.h \
                $(srcdir)/lookup-vptree-float32.c \
                $(srcdir)/lookup-vptree-float32.h \
                $(srcdir)/lookup-policy.h \
                $(srcdir)/lookup-policy-normal.h \
                $(srcdir)/lookup-policy-mono-darkbg.h \
                $(srcdir)/lookup-policy-mono-lightbg.h \
                $(srcdir)/lookup-policy-certlut.h \
                $(srcdir)/lookup-policy-5bit.h \
                $(srcdir)/lookup-policy-6bit.h \
                $(srcdir)/lookup-policy-eytzinger.h \
                $(srcdir)/lookup-policy-fhedt.h \
                $(srcdir)/lookup-policy-vptree.h \
                $(srcdir)/lookup-policy-rbc.h \
                $(srcdir)/lookup-policy-mahalanobis.h \
                $(srcdir)/factory.h \
                $(srcdir)/components.h \
                $(srcdir)/palette-heckbert.c \
                $(srcdir)/palette-heckbert.h \
                $(srcdir)/palette-common-merge.c \
                $(srcdir)/palette-common-merge.h \
                $(srcdir)/palette-common-snap.c \
                $(srcdir)/palette-common-snap.h \
                $(srcdir)/palette-kmeans.c \
                $(srcdir)/palette-kmeans-assign.c \
                $(srcdir)/palette-kmeans-assign.h \
                $(srcdir)/palette-kmeans-build.c \
                $(srcdir)/palette-kmeans-build.h \
                $(srcdir)/palette-kmeans.h \
                $(srcdir)/palette-kmedoids.c \
                $(srcdir)/palette-kmedoids.h \
                $(srcdir)/palette-kcenter.c \
                $(srcdir)/palette-kcenter.h \
                $(srcdir)/palette.c \
                $(srcdir)/palette.h \
                $(srcdir)/dither.c \
                $(srcdir)/dither.h \
                $(srcdir)/dither-common-pipeline.c \
                $(srcdir)/dither-common-pipeline.h \
                $(srcdir)/bluenoise_64x64.h \
                $(srcdir)/dither-positional-8bit.c \
                $(srcdir)/dither-positional-8bit.h \
                $(srcdir)/dither-positional-float32.c \
                $(srcdir)/dither-positional-float32.h \
                $(srcdir)/dither-fixed-8bit.c \
                $(srcdir)/dither-fixed-8bit.h \
                $(srcdir)/dither-interframe-method.c \
                $(srcdir)/dither-interframe-method.h \
                $(srcdir)/dither-interframe-stbn-source.c \
                $(srcdir)/dither-interframe-stbn-source.h \
                $(srcdir)/dither-interframe-stbn-source-hash.c \
                $(srcdir)/dither-interframe-stbn-source-hash.h \
                $(srcdir)/dither-interframe-stbn-source-mask.c \
                $(srcdir)/dither-interframe-stbn-source-mask.h \
                $(srcdir)/dither-interframe-stbn-source-mask-table.c \
                $(srcdir)/dither-interframe-stbn-source-mask-table.h \
                $(srcdir)/dither-interframe-stbn-source-pmj.c \
                $(srcdir)/dither-interframe-stbn-source-pmj.h \
                $(srcdir)/dither-fixed-float32.c \
                $(srcdir)/dither-fixed-float32.h \
                $(srcdir)/dither-internal.h \
                $(srcdir)/dither-varcoeff-8bit.c \
                $(srcdir)/dither-varcoeff-8bit.h \
                $(srcdir)/dither-varcoeff-float32.c \
                $(srcdir)/dither-varcoeff-float32.h \
                $(srcdir)/logger.c \
                $(srcdir)/logger.h \
                $(srcdir)/colorspace.c \
                $(srcdir)/colorspace.h \
                $(srcdir)/frame.c \
                $(srcdir)/frame.h \
                $(srcdir)/cpu.c \
                $(srcdir)/cpu.h \
                $(srcdir)/pixelformat.c \
                $(srcdir)/pixelformat.h \
                $(srcdir)/filter-colors.c \
                $(srcdir)/filter-colors.h \
                $(srcdir)/filter-load.c \
                $(srcdir)/filter-load.h \
                $(srcdir)/filter-factory.c \
                $(srcdir)/filter-factory.h \
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
                $(srcdir)/cms.c \
                $(srcdir)/cms.h \
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
                $(srcdir)/loader-chain.c \
                $(srcdir)/loader-factory.c \
                $(srcdir)/loader-order-schema.c \
                $(srcdir)/loader-manager.c \
                $(srcdir)/loader-component.c \
                $(srcdir)/loader-registry.c \
                $(srcdir)/loader.h \
                $(srcdir)/loader-builtin.h \
                $(srcdir)/loader-common.h \
                $(srcdir)/loader-component.h \
                $(srcdir)/loader-chain.h \
                $(srcdir)/loader-factory.h \
                $(srcdir)/loader-order-schema.h \
                $(srcdir)/loader-manager.h \
                $(srcdir)/loader-coregraphics.h \
                $(srcdir)/loader-gd.h \
                $(srcdir)/loader-gdk-pixbuf2.h \
                $(srcdir)/loader-gnome-thumbnailer.h \
                $(srcdir)/loader-libjpeg.h \
                $(srcdir)/loader-libpng.h \
                $(srcdir)/loader-libtiff.h \
                $(srcdir)/loader-librsvg.h \
                $(srcdir)/loader-librsvg-test.h \
                $(srcdir)/loader-libwebp.h \
                $(srcdir)/loader-quicklook.h \
                $(srcdir)/loader-registry.h \
                $(srcdir)/loader-wic.h \
                $(srcdir)/frompnm.c \
                $(srcdir)/frompnm.h \
                $(srcdir)/frompng.c \
                $(srcdir)/frompng.h \
                $(srcdir)/fromhdr.c \
                $(srcdir)/fromhdr.h \
                $(srcdir)/frompsd.c \
                $(srcdir)/frompsd.h \
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
                $(srcdir)/options.c \
                $(srcdir)/options.h \
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
                $(srcdir)/probe.c \
                $(srcdir)/probe.h \
                $(srcdir)/sixel_decode_rgba.c \
                $(srcdir)/sixel_decode_rgba.h \
                $(srcdir)/sixel_atomic_fallback.c \
                $(srcdir)/sixel_atomic.h \
                $(srcdir)/classid.h \
                $(srcdir)/rgblookup.h \
                $(srcdir)/lso2.h
if SIXEL_ENABLE_THREADS
libsixel_la_SOURCES += \
$(srcdir)/threadpool.c \
$(srcdir)/threadpool.h
endif
if HAVE_QUICKLOOK_THUMBNAILING
libsixel_la_SOURCES += \
$(srcdir)/quicklook_thumbnailing.m
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
libsixel_la_CPPFLAGS = -I$(top_builddir)/include/ -I$(top_srcdir) $(AM_CPPFLAGS) \
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
                $(CLIPBOARD_CFLAGS) \
                @THREAD_CFLAGS@
if BUILDING_ON_WINDOWS
libsixel_la_LDFLAGS = -no-undefined -version-info $(LS_LTVERSION) \
                $(MAYBE_COVERAGE) $(LIBSIXEL_LDFLAGS)
else
libsixel_la_LDFLAGS = -version-info $(LS_LTVERSION) \
                $(MAYBE_COVERAGE) $(LIBSIXEL_LDFLAGS)
endif
libsixel_la_LIBADD = -lm \
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
                $(CLIPBOARD_LIBS) \
                @THREAD_LIBS@

dist_man_MANS = $(srcdir)/sixel.5
EXTRA_DIST = $(srcdir)/rgblookup.gperf \
             $(srcdir)/classid.gperf

.PHONY: libobjs

# Build only the libtool object set for libsixel.
#
# This target lets the top-level Makefile overlap executable object
# compilation in other directories with the libsixel link step.
libobjs: $(libsixel_la_OBJECTS)

GPERF_OPT=--ignore-case --language=ANSI-C
GPERF_CLASSID_OPT=--language=ANSI-C \
                  -H sixel_lookup_policy_classid_hash \
                  -W sixel_lookup_policy_classid_wordlist

LOOKUP_POLICY_CLASSID_INPUTS = \
    $(srcdir)/lookup-policy-5bit.h \
    $(srcdir)/lookup-policy-6bit.h \
    $(srcdir)/lookup-policy-certlut.h \
    $(srcdir)/lookup-policy-eytzinger.h \
    $(srcdir)/lookup-policy-fhedt.h \
    $(srcdir)/lookup-policy-mahalanobis.h \
    $(srcdir)/lookup-policy-mono-darkbg.h \
    $(srcdir)/lookup-policy-mono-lightbg.h \
    $(srcdir)/lookup-policy-normal.h \
    $(srcdir)/lookup-policy-rbc.h \
    $(srcdir)/lookup-policy-vptree.h

# Keep this recipe tab-indented; GNU make requires tabs for commands.
gen-rgblookup: $(srcdir)/rgblookup.gperf
	gperf -C -N lookup_rgb $(GPERF_OPT) $<  |\
	sed 's/{""}/\{"", 0, 0, 0\}/g'          |\
	astyle                                  > $(srcdir)/rgblookup.h

# Keep this recipe tab-indented; GNU make requires tabs for commands.
gen-classid: $(LOOKUP_POLICY_CLASSID_INPUTS)
	$(AWK) -f $(top_srcdir)/tools/gen_lookup_classid_gperf.awk $^ \
	        > $(srcdir)/classid.gperf
	gperf -C -N sixel_lookup_policy_classid_lookup $(GPERF_CLASSID_OPT) \
	        $(srcdir)/classid.gperf                                    |\
	sed 's/{""}/\{"", 0\}/g'                                           \
	        > $(srcdir)/classid.h
	printf '%s\n' '#undef TOTAL_KEYWORDS'                              \
	              '#undef MIN_WORD_LENGTH'                             \
	              '#undef MAX_WORD_LENGTH'                             \
	              '#undef MIN_HASH_VALUE'                              \
	              '#undef MAX_HASH_VALUE'                              \
	        >> $(srcdir)/classid.h



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
