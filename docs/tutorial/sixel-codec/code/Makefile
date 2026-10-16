# Build the SIXEL codec tutorial programs with any POSIX make:
#
#   make            build step1 ... step5
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
STEPS = $(O)/step1 $(O)/step2 $(O)/step3 $(O)/step4 $(O)/step5

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
$(O)/step5: step5.c step4.c
	$(CC) $(CFLAGS) -o $@ common.c step5.c $(LDLIBS)

# Not in `all`: it is yours to finish, so it fails its self test at first.
mysixel: mysixel.c common.c tutorial.h
	$(CC) $(CFLAGS) -o $(O)/mysixel common.c mysixel.c $(LDLIBS)

check: all
	./check.sh $(O)

bench: all
	$(O)/step1 synth two 2400 1800 > $(O)/two.ppm
	$(O)/step1 synth noise 2400 1800 > $(O)/noise.ppm
	@for s in $(STEPS); do \
	    echo "$$s photo-600 $$($$s bench ../../../../images/snake.ppm 1 10)"; \
	    echo "$$s photo-2400 $$($$s bench ../../../../images/snake.ppm 4 10)"; \
	    echo "$$s two-2400 $$($$s bench $(O)/two.ppm 1 10)"; \
	    echo "$$s noise-2400 $$($$s bench $(O)/noise.ppm 1 10)"; \
	done

clean:
	rm -f $(STEPS) $(O)/mysixel $(O)/two.ppm $(O)/noise.ppm

.PHONY: all check bench clean
