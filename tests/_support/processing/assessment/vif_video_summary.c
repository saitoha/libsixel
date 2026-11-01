/* SPDX-License-Identifier: MIT */
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* The fixture has identity then flat target scores 1 and 0, held for
 * 0.1 and 0.3 seconds. Plain averaging (0.5) and last-only (0) must fail. */
static int
vif_number(char const *text, char const *key, double expected)
{
    char const *p;
    char *end;
    double actual;

    if (text == NULL || (p = strstr(text, key)) == NULL) return 0;
    p += strlen(key);
    actual = strtod(p, &end);
    return end != p && (*end == ',' || *end == '}') && isfinite(actual)
        && fabs(actual - expected) < 1e-12;
}

int
test_vif_video_summary(int argc, char **argv)
{
    char const *text;

    if (argc != 2) return 1;
    text = argv[1];
    return !(strstr(text, "\"complete\":true") != NULL &&
        vif_number(text, "\"frames\":", 2) &&
        vif_number(text, "\"duration\":", .4) &&
        vif_number(strstr(text, "\"quality\":"), "\"VIF-P\":", .25) &&
        vif_number(strstr(text, "\"temporal_min\":"), "\"VIF-P\":", 0) &&
        vif_number(strstr(text, "\"temporal_max\":"), "\"VIF-P\":", 1));
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
