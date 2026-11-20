# Build the SIXEL codec tutorial programs with any POSIX make:
#
#   make            build one program per chapter, first-codec ... overpaint
#   make mysixel    build your own codec from mysixel.c
#   make check      run every self test and cross-chapter comparison
#   make bench      time every chapter on the inputs of the published tables
#
# The programs do not use libsixel. Override CC, CFLAGS or O (the output
# directory) as needed, for example: make O=/tmp/sixel-tutorial check

CC = cc
CFLAGS = -O2 -Wall -Wextra
LDLIBS = -lpthread
O = .
CHAPTERS = $(O)/first-codec $(O)/nodes $(O)/packing $(O)/full-decoder \
	$(O)/fewer-questions $(O)/masks $(O)/next-node $(O)/occupancy \
	$(O)/direct-writes $(O)/word-decoder $(O)/vector-lanes $(O)/transpose \
	$(O)/parallel-bands $(O)/parallel-decoder $(O)/overpaint

all: $(CHAPTERS)

$(CHAPTERS): common.c tutorial.h
$(O)/first-codec: first-codec.c
	$(CC) $(CFLAGS) -o $@ common.c first-codec.c $(LDLIBS)
$(O)/nodes: nodes.c
	$(CC) $(CFLAGS) -o $@ common.c nodes.c $(LDLIBS)
$(O)/packing: packing.c
	$(CC) $(CFLAGS) -o $@ common.c packing.c $(LDLIBS)
$(O)/full-decoder: full-decoder.c
	$(CC) $(CFLAGS) -o $@ common.c full-decoder.c $(LDLIBS)
$(O)/fewer-questions: fewer-questions.c
	$(CC) $(CFLAGS) -o $@ common.c fewer-questions.c $(LDLIBS)
$(O)/masks: masks.c
	$(CC) $(CFLAGS) -o $@ common.c masks.c $(LDLIBS)
$(O)/next-node: next-node.c
	$(CC) $(CFLAGS) -o $@ common.c next-node.c $(LDLIBS)
$(O)/occupancy: occupancy.c
	$(CC) $(CFLAGS) -o $@ common.c occupancy.c $(LDLIBS)
$(O)/direct-writes: direct-writes.c
	$(CC) $(CFLAGS) -o $@ common.c direct-writes.c $(LDLIBS)
$(O)/word-decoder: word-decoder.c
	$(CC) $(CFLAGS) -o $@ common.c word-decoder.c $(LDLIBS)
$(O)/vector-lanes: vector-lanes.c
	$(CC) $(CFLAGS) -o $@ common.c vector-lanes.c $(LDLIBS)
$(O)/transpose: transpose.c
	$(CC) $(CFLAGS) -o $@ common.c transpose.c $(LDLIBS)
$(O)/parallel-bands: parallel-bands.c transpose.c
	$(CC) $(CFLAGS) -o $@ common.c parallel-bands.c $(LDLIBS)
$(O)/parallel-decoder: parallel-decoder.c transpose.c
	$(CC) $(CFLAGS) -o $@ common.c parallel-decoder.c $(LDLIBS)
$(O)/overpaint: overpaint.c
	$(CC) $(CFLAGS) -o $@ common.c overpaint.c $(LDLIBS)

# Not in `all`: it is yours to finish, so it fails its self test at first.
mysixel: mysixel.c common.c tutorial.h
	$(CC) $(CFLAGS) -o $(O)/mysixel common.c mysixel.c $(LDLIBS)

check: all
	./check.sh $(O)

# Photographs with an adaptive 256-color palette and error diffusion, as
# libsixel writes them, plus a two-color pattern and random noise.
STUDY = ../../../encoder/output/high-color/study
RUNS = 10
bench: all
	$(O)/full-decoder decode $(STUDY)/snake-256-fs.six > $(O)/photo-small.ppm
	$(O)/full-decoder decode $(STUDY)/snake-fhd-256-fs.six > $(O)/photo-fhd.ppm
	$(O)/first-codec synth two 1920 1080 > $(O)/two.ppm
	$(O)/first-codec synth noise 1920 1080 > $(O)/noise.ppm
	@for s in $(CHAPTERS); do \
	    for i in photo-small photo-fhd two noise; do \
	        echo "$$(basename $$s) $$i $$($$s bench $(O)/$$i.ppm 1 $(RUNS))"; \
	    done; \
	done

clean:
	rm -f $(CHAPTERS) $(O)/mysixel $(O)/photo-small.ppm $(O)/photo-fhd.ppm \
	    $(O)/two.ppm $(O)/noise.ppm

.PHONY: all check bench clean
