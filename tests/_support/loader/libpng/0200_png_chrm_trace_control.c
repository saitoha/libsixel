/* SPDX-License-Identifier: MIT */
/* Supply controlled records to the real TAP checker, without decoding PNG. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
test_loader_0200_png_chrm_trace_control(int argc, char **argv)
{
    char const *fault;
    char const *engine;
    char const *path;
    char const *event;
    char const *suffix;
    int fixture;
    int count;
    int i;
    int src;
    int dst;
    int ok;

    if (argc >= 2 && strcmp(argv[1], "exit77") == 0) {
        return 77;
    }
    if (argc != 5 ||
        strcmp(argv[2], "loader/0198_png_chrm_samples") != 0) {
        return 1;
    }
    fault = argv[1];
    fixture = atoi(argv[3]);
    /* The selected file-background TAP uses fixture 70; image controls
     * use one record. Direct/none controls intentionally receive one. */
    count = fixture == 70 ? 2 : 1;
    if (strcmp(fault, "missing-background") == 0) {
        --count;
    }
    if (strcmp(fault, "extra-apply") == 0) {
        ++count;
    }
    for (i = 0; i < count; ++i) {
        engine = argv[4];
        path = strcmp(engine, "builtin") == 0 ? "extended-rgb" : "external";
        event = "apply";
        suffix = "";
        src = 4;
        dst = 4;
        ok = 1;
        /* Only the background record changes; the image remains valid. */
        if (i == 1) {
            if (strcmp(fault, "wrong-background-engine") == 0) {
                engine = "builtin";
                path = "extended-rgb";
            }
            if (strcmp(fault, "renamed-background") == 0) {
                event = "background";
            }
            if (strcmp(fault, "wrong-background-path") == 0) {
                path = "bounded-device";
            }
            if (strcmp(fault, "wrong-background-source") == 0) {
                src = 3;
            }
            if (strcmp(fault, "wrong-background-destination") == 0) {
                dst = 3;
            }
            if (strcmp(fault, "failed-background") == 0) {
                ok = 0;
            }
            if (strcmp(fault, "extra-background-field") == 0) {
                suffix = "|extra=1";
            }
        }
        fprintf(stderr, "libsixel[loader]: LSXCMS2|event=%s|engine=%s|"
            "path=%s|src=%d|dst=%d|ok=%d%s\n",
            event, engine, path, src, dst, ok, suffix);
    }
    /* The real numeric entry's marker deliberately cannot substitute for
     * checking every apply record in the enclosing TAP. */
    fprintf(stderr, "PNG_CHRM_SAMPLE|fixture=%d|engine=%s|checked=24|"
        "interpretation=4\n", fixture, argv[4]);
    return 0;
}
