# Build the dithering tutorial programs with any POSIX make:
#
#   make            build step1 ... step10 and the dtool helper
#   make mydither   build your own dither from mydither.c
#   make check      run every self test and cross-step comparison
#
# The programs do not use libsixel. Override CC, CFLAGS or O (the output
# directory) as needed, for example: make O=/tmp/dither-tutorial check
#
# -ffp-contract=off keeps floating-point results the same on processors with
# and without a fused multiply-add instruction.

CC = cc
CFLAGS = -O2 -Wall -Wextra -ffp-contract=off
O = .
STEPS = $(O)/step1 $(O)/step2 $(O)/step3 $(O)/step4 $(O)/step7 $(O)/step8 \
	$(O)/step9 $(O)/step10
LIB = common.c main.c

all: $(STEPS) $(O)/dtool

$(STEPS): $(LIB) tutorial.h
$(O)/step1: step1.c
	$(CC) $(CFLAGS) -o $@ $(LIB) step1.c
$(O)/step2: step2.c
	$(CC) $(CFLAGS) -o $@ $(LIB) step2.c
$(O)/step3: step3.c
	$(CC) $(CFLAGS) -o $@ $(LIB) step3.c
$(O)/step4: step4.c
	$(CC) $(CFLAGS) -o $@ $(LIB) step4.c
$(O)/step7: step7.c
	$(CC) $(CFLAGS) -o $@ $(LIB) step7.c -lm
$(O)/step8: step8.c
	$(CC) $(CFLAGS) -o $@ $(LIB) step8.c
$(O)/step9: step9.c
	$(CC) $(CFLAGS) -o $@ $(LIB) step9.c -lm
$(O)/step10: step10.c
	$(CC) $(CFLAGS) -pthread -o $@ $(LIB) step10.c
$(O)/dtool: dtool.c common.c tutorial.h
	$(CC) $(CFLAGS) -o $@ common.c dtool.c -lm

# Not in `all`: it is yours to finish.
mydither: mydither.c $(LIB) tutorial.h
	$(CC) $(CFLAGS) -o $(O)/mydither $(LIB) mydither.c

check: all
	./check.sh $(O)

clean:
	rm -f $(STEPS) $(O)/dtool $(O)/mydither $(O)/*.ppm

.PHONY: all check clean
