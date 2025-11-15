/*
 * SPDX-License-Identifier: MIT
 *
 * Internal helper that installs a SIGABRT handler for CLI tools when the
 * signal is currently unhandled.  The handler prints a trimmed stack trace
 * before letting the default disposition terminate the process.
 */

#ifndef LIBSIXEL_CONVERTERS_ABORTTRACE_H
#define LIBSIXEL_CONVERTERS_ABORTTRACE_H

#ifdef __cplusplus
extern "C" {
#endif

void sixel_aborttrace_install_if_unhandled(void);

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_CONVERTERS_ABORTTRACE_H */
