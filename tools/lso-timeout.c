/*
 * lso-timeout.c - minimal timeout helper with graceful termination
 *
 * This utility mirrors the subset of GNU timeout used by the test
 * driver. It terminates the child process with SIGTERM after the
 * specified deadline and escalates to SIGKILL after the grace period
 * given via -k.
 */

#if !defined(_POSIX_C_SOURCE)
# define _POSIX_C_SOURCE 200809L
#endif

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(HAVE_TIME_H)
# include <time.h>
#endif  /* defined(HAVE_TIME_H) */
#if defined(HAVE_ERRNO_H)
# include <errno.h>
#endif  /* defined(HAVE_ERRNO_H) */

#if defined(HAVE_FORK) && defined(HAVE_WAITPID) && defined(HAVE_SYS_WAIT_H)
# define LSO_HAVE_POSIX_TIMEOUT 1
#endif

#if defined(HAVE_WINDOWS_PROC)
# define LSO_HAVE_WINDOWS_TIMEOUT 1
#endif

#if defined(LSO_HAVE_WINDOWS_TIMEOUT) && defined(LSO_HAVE_POSIX_TIMEOUT)
# undef LSO_HAVE_WINDOWS_TIMEOUT
#endif

#if defined(LSO_HAVE_WINDOWS_TIMEOUT)
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
#endif

#if defined(HAVE_UNISTD_H)
# include <unistd.h>
#endif  /* defined(HAVE_UNISTD_H) */

#if defined(LSO_HAVE_POSIX_TIMEOUT)
# if defined(HAVE_SIGNAL_H)
#  include <signal.h>
# endif  /* defined(HAVE_SIGNAL_H) */
# if defined(HAVE_SYS_TYPES_H)
#  include <sys/types.h>
# endif  /* defined(HAVE_SYS_TYPES_H) */
# if defined(HAVE_SYS_WAIT_H)
#  include <sys/wait.h>
# endif  /* defined(HAVE_SYS_WAIT_H) */
# if defined(HAVE_SYS_TIME_H)
#  include <sys/time.h>
# endif  /* defined(HAVE_SYS_TIME_H) */
#endif  /* defined(LSO_HAVE_POSIX_TIMEOUT) */

#if defined(LSO_HAVE_POSIX_TIMEOUT) || defined(LSO_HAVE_WINDOWS_TIMEOUT)
static double
current_time_seconds(void)
{
# if defined(LSO_HAVE_WINDOWS_TIMEOUT)
  ULONGLONG ticks;

  ticks = GetTickCount64();
  return (double) ticks / 1000.0;
# else
#  if defined(CLOCK_MONOTONIC)
  struct timespec ts;

  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
    return -1.0;
  }

  return (double) ts.tv_sec + (double) ts.tv_nsec / 1.0e9;
#  elif defined(HAVE_SYS_TIME_H)
  struct timeval tv;

  if (gettimeofday(&tv, NULL) != 0) {
    return -1.0;
  }

  return (double) tv.tv_sec + (double) tv.tv_usec / 1.0e6;
#  else
  return -1.0;
#  endif
# endif
}
#endif

static int
parse_duration(const char *text, double *out)
{
  char *endptr;
  double value;

  if (text == NULL || out == NULL) {
    return -1;
  }

  value = strtod(text, &endptr);
  if (endptr == text) {
    return -1;
  }

  if (*endptr != '\0') {
    if (strcmp(endptr, "s") != 0) {
      return -1;
    }
  }

  if (value < 0.0) {
    return -1;
  }

  *out = value;
  return 0;
}

static void
usage(void)
{
  fprintf(stderr, "Usage: lso-timeout [-k DURATION] DURATION COMMAND...\n");
  exit(EXIT_FAILURE);
}

#if defined(LSO_HAVE_POSIX_TIMEOUT)
static void
sleep_millis(long millis)
{
# if defined(HAVE_NANOSLEEP)
  struct timespec req;
  struct timespec rem;

  req.tv_sec = millis / 1000;
  req.tv_nsec = (millis % 1000) * 1000000L;

  while (nanosleep(&req, &rem) != 0) {
    if (errno != EINTR) {
      break;
    }
    req = rem;
  }
# else
  if (millis <= 0) {
    return;
  }
  usleep((useconds_t) millis * 1000U);
# endif
}

static int
wait_with_timeout(pid_t child, double deadline, double kill_deadline)
{
  int status;
  int term_sent;
  double now;

  term_sent = 0;

  for (;;) {
    if (waitpid(child, &status, WNOHANG) == child) {
      if (term_sent != 0) {
        return 124;
      }
      if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
      }
      if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
      }
      return EXIT_FAILURE;
    }

    now = current_time_seconds();
    if (now < 0.0) {
      kill(child, SIGKILL);
      return EXIT_FAILURE;
    }

    if (term_sent == 0 && now >= deadline) {
      kill(child, SIGTERM);
      term_sent = 1;
    } else if (term_sent != 0 && kill_deadline >= 0.0 &&
               now >= kill_deadline) {
      kill(child, SIGKILL);
      kill_deadline = -1.0;
    }

    sleep_millis(10);
  }
}
#endif  /* defined(LSO_HAVE_POSIX_TIMEOUT) */

#if defined(LSO_HAVE_WINDOWS_TIMEOUT)
static size_t
command_line_length(char **argv)
{
  size_t length;
  size_t i;
  const char *p;

  /*
   * Each argument is always quoted to preserve whitespace and shell
   * metacharacters. Quotes and backslashes are escaped so the Windows
   * command-line parser reconstructs the original argv array.
   */
  length = 1;
  for (i = 0; argv[i] != NULL; i++) {
    if (i != 0) {
      length++;
    }
    length += 2;
    for (p = argv[i]; *p != '\0'; p++) {
      if (*p == '"' || *p == '\\') {
        length++;
      }
      length++;
    }
  }

  return length;
}

static void
copy_command_line(char *dst, char **argv)
{
  char *cur;
  size_t i;
  const char *p;

  cur = dst;
  for (i = 0; argv[i] != NULL; i++) {
    if (i != 0) {
      *cur++ = ' ';
    }
    *cur++ = '"';
    for (p = argv[i]; *p != '\0'; p++) {
      if (*p == '"' || *p == '\\') {
        *cur++ = '\\';
      }
      *cur++ = *p;
    }
    *cur++ = '"';
  }
  *cur = '\0';
}

static HANDLE
spawn_process(char **argv)
{
  STARTUPINFOA si;
  PROCESS_INFORMATION pi;
  size_t length;
  char *cmdline;

  /*
   * CreateProcessA requires a single command-line string. We build it
   * from argv with quoting and escaping to match the POSIX execvp
   * behaviour used on other platforms.
   */
  length = command_line_length(argv);
  cmdline = (char *) malloc(length);
  if (cmdline == NULL) {
    fprintf(stderr, "lso-timeout: out of memory\n");
    return NULL;
  }

  copy_command_line(cmdline, argv);

  memset(&si, 0, sizeof(si));
  si.cb = sizeof(si);
  memset(&pi, 0, sizeof(pi));

  if (CreateProcessA(NULL, cmdline, NULL, NULL, TRUE, 0, NULL, NULL, &si,
                     &pi) == 0) {
    fprintf(stderr, "lso-timeout: CreateProcessA failed: %lu\n",
            (unsigned long) GetLastError());
    free(cmdline);
    return NULL;
  }

  CloseHandle(pi.hThread);
  free(cmdline);
  return pi.hProcess;
}

static int
wait_with_timeout_win(HANDLE child, double deadline, double kill_deadline)
{
  DWORD wait_ms;
  DWORD wait_result;
  DWORD exit_code;
  double now;
  double next_deadline;
  int term_sent;

  term_sent = 0;

  for (;;) {
    now = current_time_seconds();
    if (now < 0.0) {
      TerminateProcess(child, 1);
      CloseHandle(child);
      return EXIT_FAILURE;
    }

    /*
     * Wait in short slices so we can reevaluate whether the soft
     * deadline has passed and send termination promptly.
     */
    next_deadline = deadline;
    if (term_sent != 0 && kill_deadline >= 0.0 &&
        kill_deadline < next_deadline) {
      next_deadline = kill_deadline;
    }

    if (now >= next_deadline) {
      wait_ms = 0;
    } else {
      wait_ms = (DWORD) ((next_deadline - now) * 1000.0);
      if (wait_ms > 50) {
        wait_ms = 50;
      }
    }

    wait_result = WaitForSingleObject(child, wait_ms);
    if (wait_result == WAIT_OBJECT_0) {
      if (GetExitCodeProcess(child, &exit_code) == 0) {
        CloseHandle(child);
        return EXIT_FAILURE;
      }
      CloseHandle(child);
      if (term_sent != 0) {
        return 124;
      }
      return (int) exit_code;
    }

    if (wait_result != WAIT_TIMEOUT) {
      TerminateProcess(child, 1);
      CloseHandle(child);
      return EXIT_FAILURE;
    }

    now = current_time_seconds();
    if (now < 0.0) {
      TerminateProcess(child, 1);
      CloseHandle(child);
      return EXIT_FAILURE;
    }

    if (term_sent == 0 && now >= deadline) {
      TerminateProcess(child, 1);
      term_sent = 1;
    } else if (term_sent != 0 && kill_deadline >= 0.0 &&
               now >= kill_deadline) {
      TerminateProcess(child, 1);
      kill_deadline = -1.0;
    }
  }
}
#endif  /* defined(LSO_HAVE_WINDOWS_TIMEOUT) */

int
main(int argc, char **argv)
{
  double timeout_seconds;
  double kill_delay;
  int argi;
#if defined(LSO_HAVE_POSIX_TIMEOUT)
  double deadline;
  double kill_deadline;
  pid_t pid;
#elif defined(LSO_HAVE_WINDOWS_TIMEOUT)
  double deadline;
  double kill_deadline;
  HANDLE child;
#endif

  timeout_seconds = -1.0;
  kill_delay = 10.0;
  argi = 1;

  if (argc < 3) {
    usage();
  }

  if (strcmp(argv[argi], "-k") == 0) {
    if (argc < 5) {
      usage();
    }
    if (parse_duration(argv[argi + 1], &kill_delay) != 0) {
      usage();
    }
    argi += 2;
  }

  if (parse_duration(argv[argi], &timeout_seconds) != 0) {
    usage();
  }
  argi++;

#if defined(LSO_HAVE_POSIX_TIMEOUT)
  pid = fork();
  if (pid < 0) {
    perror("fork");
    return EXIT_FAILURE;
  }

  if (pid == 0) {
    execvp(argv[argi], &argv[argi]);
    perror("execvp");
    _exit(EXIT_FAILURE);
  }

  deadline = current_time_seconds();
  if (deadline < 0.0) {
    kill(pid, SIGKILL);
    return EXIT_FAILURE;
  }
  deadline += timeout_seconds;

  kill_deadline = deadline + kill_delay;

  return wait_with_timeout(pid, deadline, kill_deadline);
#elif defined(LSO_HAVE_WINDOWS_TIMEOUT)
  child = spawn_process(&argv[argi]);
  if (child == NULL) {
    return EXIT_FAILURE;
  }

  deadline = current_time_seconds();
  if (deadline < 0.0) {
    TerminateProcess(child, 1);
    CloseHandle(child);
    return EXIT_FAILURE;
  }
  deadline += timeout_seconds;

  kill_deadline = deadline + kill_delay;

  return wait_with_timeout_win(child, deadline, kill_deadline);
#else
  fprintf(stderr, "lso-timeout: timeout support is disabled\n");
  return EXIT_FAILURE;
#endif
}
