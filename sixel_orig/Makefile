CC = cc
CFLAGS = -O3 -g -I/usr/local/include
LFLAGS = -L/usr/local/lib -lgd

OBJS = main.o tosixel.o fromsixel.o frompnm.o
PROG = sixel

$(PROG)	:	$(OBJS)
	$(CC) $(OBJS) $(LFLAGS) -o $(PROG)

.c.o	:
	$(CC) $(CFLAGS) -c $<
