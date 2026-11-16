/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_TEST_PTY_CHILD_EXEC_H
#define LIBSIXEL_TEST_PTY_CHILD_EXEC_H

#include <unistd.h>

/* A direct exec call lets gcov write the whole runner's profile before the
 * target starts. Keep those writes outside the scripted PTY deadlines. */
static int
test_pty_child_exec(char const *path, char *const arguments[])
{
    int (*volatile execute)(char const *, char *const[]) = execv;

    return execute(path, arguments);
}

#endif
