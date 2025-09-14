/*
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef LIBSIXEL_GETOPT_STUB_H
# define LIBSIXEL_GETOPT_STUB_H

# if HAVE_GETOPT_H
#  include <getopt.h>
# else

#  include <stdio.h>
#  include <string.h>

/* provide long option interface */
struct option {
    const char *name;
    int has_arg;
    int *flag;
    int val;
};

/* argument flags */
#  define no_argument        0
#  define required_argument  1
#  define optional_argument  2

/* global variables as in libc getopt */
static char *optarg;
static int optind = 1;
static int opterr = 1;
static int optopt;

/* internal state for getopt */
static char *nextchar;

static int
getopt(int argc, char * const argv[], const char *optstring)
{
    const char *opt_ptr;
    int c;
    char *arg;

    if (optind >= argc || argv[optind] == NULL) {
        return -1;
    }

    if (!nextchar || *nextchar == '\0') {
        arg = argv[optind];
        if (arg[0] != '-' || arg[1] == '\0') {
            return -1;
        }
        if (strcmp(arg, "--") == 0) {
            optind++;
            return -1;
        }
        nextchar = arg + 1;
        ++optind;
    }

    c = *nextchar++;
    opt_ptr = strchr(optstring, c);
    if (! opt_ptr) {
        optopt = c;
        if (opterr && *optstring != ':') {
            fprintf(stderr, "unknown option '-%c'\n", c);
        }
        return '?';
    }
    if (opt_ptr[1] == ':') {
        if (*nextchar != '\0') {
            optarg = (char *)nextchar;
            nextchar = NULL;
        } else if (optind < argc) {
            optarg = argv[optind];
            ++optind;
            nextchar = NULL;
        } else {
            optopt = c;
            if (*optstring == ':') {
                return ':';
            }
            if (opterr) {
                fprintf(stderr, "option '-%c' requires an argument\n", c);
            }
            return '?';
        }
    } else {
        optarg = NULL;
        if (*nextchar == '\0') {
            nextchar = NULL;
        }
    }
    return c;
}

static int
getopt_long(int argc,
            char * const argv[],
            const char *optstring,
            const struct option *longopts,
            int *longindex)
{
    char *arg;
    const struct option *o;
    int i;
    char *name;
    char *value;
    size_t namelen;

    if (optind >= argc) {
        return -1;
    }

    arg = argv[optind];
    if (arg[0] != '-') {
        return -1;
    }
    if (strcmp(arg, "--") == 0) {
        optind++;
        return -1;
    }

    if (arg[1] == '-') {
        name = arg + 2;
        value = strchr(name, '=');
        namelen = value ? (size_t)(value - name) : strlen(name);
        for (i = 0; longopts[i].name; ++i) {
            o = &longopts[i];
            if (strlen(o->name) == namelen && strncmp(name, o->name, namelen) == 0) {
                if (longindex) {
                    *longindex = i;
                }
                optind++;
                if (o->has_arg == required_argument) {
                    if (value) {
                        optarg = value + 1;
                    } else if (optind < argc) {
                        optarg = argv[optind];
                        optind++;
                    } else {
                        optopt = o->val;
                        if (*optstring == ':') {
                            return ':';
                        }
                        if (opterr) {
                            fprintf(stderr, "option '--%s' requires an argument\n", o->name);
                        }
                        return '?';
                    }
                } else if (o->has_arg == optional_argument) {
                    optarg = value ? value + 1 : NULL;
                    if (!value) {
                        optarg = NULL;
                    }
                } else {
                    optarg = NULL;
                }
                if (o->flag) {
                    *(o->flag) = o->val;
                    return 0;
                }
                return o->val;
            }
        }
        ++optind;
        if (opterr) {
            fprintf(stderr, "unrecognized option '--%.*s'\n", (int)namelen, name);
        }
        return '?';
    }

    return getopt(argc, argv, optstring);
}

/* ensure long options are available */
#  ifndef HAVE_GETOPT_LONG
#   define HAVE_GETOPT_LONG 1
#  endif

# endif /* !HAVE_GETOPT_H */

#endif /* LIBSIXEL_GETOPT_STUB_H */
