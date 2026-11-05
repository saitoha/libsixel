# Build the SIXEL codec tutorial programs with any POSIX make:
#
#   make            build step1 ... step6
#   make mysixel    build your own codec from mysixel.c
#   make check      run every self test and cross-step comparison
#   make bench      time every step on the inputs of the published table
#
# The programs do not use libsixel. Override CC, CFLAGS or O (the output
# directory) as needed, for example: make O=/tmp/sixel-tutorial check

CC = cc
CFLAGS = -O2 -Wall -Wextra
LDLIBS = -lpthread
O = .
STEPS = $(O)/step1 $(O)/step2 $(O)/step3 $(O)/step4 $(O)/step5 $(O)/step6

all: $(STEPS)

$(STEPS): common.c tutorial.h
$(O)/step1: step1.c
	$(CC) $(CFLAGS) -o $@ common.c step1.c $(LDLIBS)
$(O)/step2: step2.c
	$(CC) $(CFLAGS) -o $@ common.c step2.c $(LDLIBS)
$(O)/step3: step3.c
	$(CC) $(CFLAGS) -o $@ common.c step3.c $(LDLIBS)
$(O)/step4: step4.c
	$(CC) $(CFLAGS) -o $@ common.c step4.c $(LDLIBS)
$(O)/step5: step5.c
	$(CC) $(CFLAGS) -o $@ common.c step5.c $(LDLIBS)
$(O)/step6: step6.c step5.c
	$(CC) $(CFLAGS) -o $@ common.c step6.c $(LDLIBS)

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
	$(O)/step2 decode $(STUDY)/snake-256-fs.six > $(O)/photo-small.ppm
	$(O)/step2 decode $(STUDY)/snake-fhd-256-fs.six > $(O)/photo-fhd.ppm
	$(O)/step1 synth two 1920 1080 > $(O)/two.ppm
	$(O)/step1 synth noise 1920 1080 > $(O)/noise.ppm
	@for s in $(STEPS); do \
	    for i in photo-small photo-fhd two noise; do \
	        echo "$$(basename $$s) $$i $$($$s bench $(O)/$$i.ppm 1 $(RUNS))"; \
	    done; \
	done

clean:
	rm -f $(STEPS) $(O)/mysixel $(O)/photo-small.ppm $(O)/photo-fhd.ppm \
	    $(O)/two.ppm $(O)/noise.ppm

.PHONY: all check bench clean
