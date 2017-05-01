
bin_PROGRAMS = demo
demo_SOURCES = main.c
demo_CFLAGS = -Wall -Wextra $(LIBSIXEL_CFLAGS) $(GL_CFLAGS) $(X_CFLAGS)
demo_LDADD = -lm $(LIBSIXEL_LIBS) $(GL_LIBS) $(X_LIBS)

check: check-am
	./demo
