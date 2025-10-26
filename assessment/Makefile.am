if COND_GCOV
MAYBE_COVERAGE = --coverage
endif

bin_PROGRAMS = lsqa
lsqa_SOURCES = $(srcdir)/lsqa.c
lsqa_CPPFLAGS = -I$(top_builddir)/include/ \
                -I$(top_srcdir)/src \
                $(AM_CPPFLAGS)
lsqa_CFLAGS = $(AM_CFLAGS) $(MAYBE_COVERAGE)
lsqa_LDFLAGS = $(MAYBE_COVERAGE)
lsqa_LDADD = $(top_builddir)/src/libsixel.la

if HAVE_ONNXRUNTIME
lpipsmodelsdir = @modeldir@/lpips
dist_lpipsmodels_DATA = $(top_srcdir)/models/lpips/lpips_diff.onnx \
                        $(top_srcdir)/models/lpips/lpips_feature.onnx
endif
