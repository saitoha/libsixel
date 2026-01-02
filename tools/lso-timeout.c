/*
 * lso-timeout.c - minimal timeout helper with graceful termination
 *
 * This utility mirrors the subset of GNU timeout used by the test
 * driver. It terminates the child process with SIGTERM after the
 * specified deadline and escalates to SIGKILL after the grace period
 * given via -k.
 */

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

static double
current_time_seconds(void)
{
  struct timespec ts;

  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
    return -1.0;
  }

  return (double) ts.tv_sec + (double) ts.tv_nsec / 1.0e9;
}

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

static void
sleep_millis(long millis)
{
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

int
main(int argc, char **argv)
{
  double timeout_seconds;
  double kill_delay;
  double deadline;
  double kill_deadline;
  pid_t pid;
  int argi;

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
}
