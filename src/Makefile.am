
lib_LTLIBRARIES = libsixel.la
libsixel_la_SOURCES = image.c output.c fromsixel.c tosixel.c
if WITH_WIC
libsixel_la_SOURCES += wic.cc
endif
libsixel_la_CFLAGS = -I$(top_builddir)/include/
libsixel_la_LDFLAGS = -version-info $(LS_LTVERSION)
dist_man_MANS = sixel.5

install-exec-hook:
	echo aaaaaaaaaaaaaaaaaaaaaaa
