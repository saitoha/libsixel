include_HEADERS = sixel.h 6cells.h
BUILT_SOURCES = 6cells.h
EXTRA_DIST = 6cells.idl

6cells.h: $(srcdir)/6cells.idl $(top_srcdir)/tools/gen_6cells_h.awk
	$(AM_V_GEN)$(AWK) -f $(top_srcdir)/tools/gen_6cells_h.awk \
		$(srcdir)/6cells.idl > $@
