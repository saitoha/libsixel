/*
 * Opt-in complete-command timing for lso-timeout. No TAP interpretation lives
 * here. The caller supplies the identity; the Meson reader owns the verdict.
 *
 * A detached guardian owns the command's process group and watches a lifeline
 * from the outer executor. Killing the outer process (even with SIGKILL) closes
 * that pipe, so the guardian kills and reaps the command. Commands must not
 * deliberately escape their process group with setsid/setpgid. This is process
 * supervision, not a sandbox for hostile commands.
 */
#ifndef LSO_CHILD_TIME_H
#define LSO_CHILD_TIME_H

#include <math.h>

#if defined(HAVE_LSO_CHILD_TIME)
#include <fcntl.h>
#include <poll.h>

static volatile sig_atomic_t lso_wake_fd = -1;
static volatile sig_atomic_t lso_stop_signal = 0;

static void
lso_time_signal(int signo)
{
  int saved_errno;
  unsigned char event;

  saved_errno = errno;
  event = (unsigned char) signo;
  if (signo != SIGCHLD) {
    lso_stop_signal = signo;
  }
  if (lso_wake_fd >= 0) {
    /* A full pipe already wakes poll; never block inside a signal handler. */
    (void) write((int) lso_wake_fd, &event, 1);
  }
  errno = saved_errno;
}

static int
lso_time_pipe(int fds[2])
{
  int i;

  if (pipe(fds) < 0) {
    return -1;
  }
  for (i = 0; i < 2; ++i) {
    if (fcntl(fds[i], F_SETFD, FD_CLOEXEC) < 0) {
      close(fds[0]);
      close(fds[1]);
      fds[0] = fds[1] = -1;
      return -1;
    }
  }
  return 0;
}

static int
lso_time_nonblock(int fd)
{
  int flags;

  flags = fcntl(fd, F_GETFL);
  if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
    return -1;
  }
  return flags;
}

static double
lso_time_now(void)
{
  struct timespec ts;

  /* A wall clock fallback would silently invalidate this interval. */
  if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0) {
    return -1.0;
  }
  return (double) ts.tv_sec + (double) ts.tv_nsec / 1.0e9;
}

static void
lso_time_reap(pid_t child)
{
  int status;

  while (waitpid(child, &status, 0) < 0 && errno == EINTR) {
    /* Reap even on the failure path; closing pipes alone leaves a zombie. */
  }
}

static int
lso_time_observe(pid_t child, int nohang)
{
  siginfo_t info;
  int result;

  memset(&info, 0, sizeof(info));
  do {
    result = waitid(P_PID, (id_t) child, &info,
                   WEXITED | WNOWAIT | (nohang ? WNOHANG : 0));
  } while (result < 0 && errno == EINTR);
  if (result < 0) {
    return -1;
  }
  return info.si_pid == child;
}

static int
lso_time_finish(pid_t child, int exited, int *status)
{
  int observed;
  int failed;
  pid_t waited;

  failed = 0;
  if (!exited) {
    /* The direct child has never been reaped. Stop it even before setpgid,
     * then observe exit without releasing its PID. It cannot create a group
     * after the final group signal, nor can another group reuse this ID. */
    kill(child, SIGKILL);
    observed = lso_time_observe(child, 0);
    if (observed < 0 && errno != ECHILD) {
      failed = 1;
      observed = lso_time_observe(child, 0);
    }
    exited = observed == 1;
  }
  if (exited) {
    kill(-child, SIGKILL);
  } else {
    /* Without a waitable identity, do not signal a numeric process group. */
    failed = 1;
  }
  do {
    waited = waitpid(child, status, 0);
  } while (waited < 0 && errno == EINTR);
  if (waited != child) {
    lso_time_reap(child);
    failed = 1;
  }
  /* No group or PID signal is allowed after this reap. */
  return failed ? -1 : 0;
}

struct lso_time_stream {
  int fd;
  int flags;
  size_t offset;
  size_t size;
  unsigned char bytes[65536];
};

static int
lso_time_guard(int control, double start, double duration,
               const char *name, char **command, const sigset_t *mask)
{
  int pipes[2][2];
  int wake[2];
  struct lso_time_stream streams[2];
  struct pollfd fds[6];
  struct sigaction action;
  pid_t child;
  int observed;
  int i;
  int status;
  int exited;
  int result;
  int timed_out;
  int terminating;
  int count;
  int poll_ms;
  ssize_t n;
  unsigned char events[128];
  double now;
  double end;
  double deadline;
  double stop_deadline;
  char marker[8192];
  size_t sent;

  child = -1;
  exited = 0;
  status = 0;
  result = 125;
  timed_out = terminating = 0;
  stop_deadline = 0.0;
  /* Zero delegates the deadline to the outer harness, including its timeout
   * multiplier. A positive duration is available to standalone callers. */
  deadline = duration > 0.0 ? start + duration : HUGE_VAL;
  wake[0] = wake[1] = -1;
  memset(streams, 0, sizeof(streams));
  for (i = 0; i < 2; ++i) {
    pipes[i][0] = pipes[i][1] = -1;
    streams[i].fd = streams[i].flags = -1;
  }

  /* Leave the outer harness's process group before starting the command. */
  if (setsid() < 0 || lso_time_nonblock(control) < 0 ||
      lso_time_pipe(wake) < 0 || lso_time_nonblock(wake[0]) < 0 ||
      lso_time_nonblock(wake[1]) < 0) {
    goto cleanup;
  }
  lso_wake_fd = wake[1];
  memset(&action, 0, sizeof(action));
  sigemptyset(&action.sa_mask);
  action.sa_handler = lso_time_signal;
  if (sigaction(SIGCHLD, &action, NULL) < 0 ||
      sigaction(SIGTERM, &action, NULL) < 0 ||
      sigaction(SIGINT, &action, NULL) < 0 ||
      sigaction(SIGHUP, &action, NULL) < 0) {
    goto cleanup;
  }
  for (i = 0; i < 2; ++i) {
    if (lso_time_pipe(pipes[i]) < 0) {
      goto cleanup;
    }
  }
  child = fork();
  if (child < 0) {
    goto cleanup;
  }
  if (child == 0) {
    action.sa_handler = SIG_DFL;
    /* The child is the sole group creator; concurrent parent/child creation
     * can spuriously fail on some kernels. No command runs before this. */
    if (setpgid(0, 0) < 0 ||
        dup2(pipes[0][1], STDOUT_FILENO) < 0 ||
        dup2(pipes[1][1], STDERR_FILENO) < 0 ||
        sigaction(SIGCHLD, &action, NULL) < 0 ||
        sigaction(SIGTERM, &action, NULL) < 0 ||
        sigaction(SIGINT, &action, NULL) < 0 ||
        sigaction(SIGHUP, &action, NULL) < 0 ||
        sigaction(SIGPIPE, &action, NULL) < 0 ||
        sigprocmask(SIG_SETMASK, mask, NULL) < 0) {
      _exit(125);
    }
    /* CLOEXEC closes all supervisor pipes, including the outer lifeline. */
    execvp(command[0], command);
    result = errno == ENOENT ? 127 : 126;
    perror("lso-timeout: execvp");
    _exit(result);
  }
  for (i = 0; i < 2; ++i) {
    close(pipes[i][1]);
    pipes[i][1] = -1;
    streams[i].fd = pipes[i][0];
    pipes[i][0] = -1;
    streams[i].flags = fcntl(i + 1, F_GETFL);
    if (streams[i].flags < 0 ||
        lso_time_nonblock(streams[i].fd) < 0) {
      goto cleanup;
    }
  }
  /* Save both original flags before changing potentially aliased outputs. */
  if (lso_time_nonblock(STDOUT_FILENO) < 0 ||
      lso_time_nonblock(STDERR_FILENO) < 0 ||
      sigprocmask(SIG_SETMASK, mask, NULL) < 0) {
    goto cleanup;
  }

  for (;;) {
    now = lso_time_now();
    if (now < 0.0 || now < start) {
      goto cleanup;
    }
    if (!exited) {
      observed = lso_time_observe(child, 1);
      if (observed < 0) {
        goto cleanup;
      }
      exited = observed;
    }
    /* Control is checked even when the last output and exit arrive together. */
    n = read(control, events, sizeof(events));
    if (n == 0 || (n < 0 && errno != EAGAIN && errno != EINTR)) {
      goto cleanup;
    }
    if (n > 0 && !lso_stop_signal) {
      lso_stop_signal = events[0];
    }
    if (!terminating && (lso_stop_signal || now >= deadline)) {
      timed_out = !lso_stop_signal;
      terminating = 1;
      stop_deadline = now + 0.1;
      if (kill(-child, lso_stop_signal ?
               (int) lso_stop_signal : SIGTERM) < 0 &&
          errno == ESRCH && !exited) {
        /* Before group creation, signal the still-owned child directly. */
        kill(child, lso_stop_signal ? (int) lso_stop_signal : SIGTERM);
      }
    }
    if (terminating && now >= stop_deadline) {
      if (kill(-child, SIGKILL) < 0 && errno == ESRCH && !exited) {
        kill(child, SIGKILL);
      }
      /* Output backpressure cannot prevent timeout/outer-death cleanup. An
       * incomplete transfer has no complete duration and emits no marker. */
      if (streams[0].size || streams[1].size ||
          now >= stop_deadline + 1.0) {
        goto cleanup;
      }
    }
    if (exited && streams[0].fd < 0 && streams[1].fd < 0 &&
        !streams[0].size && !streams[1].size) {
      break;
    }
    memset(fds, 0, sizeof(fds));
    fds[0].fd = control;
    fds[0].events = POLLIN;
    fds[1].fd = wake[0];
    fds[1].events = POLLIN;
    for (i = 0; i < 2; ++i) {
      fds[2 + i].fd = streams[i].size ? -1 : streams[i].fd;
      fds[2 + i].events = POLLIN;
      fds[4 + i].fd = streams[i].size ? i + 1 : -1;
      fds[4 + i].events = POLLOUT;
    }
    end = terminating ? stop_deadline : deadline;
    poll_ms = -1;
    if (isfinite(end)) {
      poll_ms = (int) ((end - now) * 1000.0) + 1;
      if (poll_ms < 1) {
        poll_ms = 1;
      }
    }
    if (poll(fds, 6, poll_ms) < 0 && errno != EINTR) {
      goto cleanup;
    }
    /* The SIGCHLD self-pipe prevents lost exits between waitid and poll. */
    while (read(wake[0], events, sizeof(events)) > 0) {
    }
    for (i = 0; i < 2; ++i) {
      if (fds[4 + i].revents) {
        n = write(i + 1, streams[i].bytes + streams[i].offset,
                  streams[i].size);
        if (n > 0) {
          streams[i].offset += (size_t) n;
          streams[i].size -= (size_t) n;
        } else if (n < 0 && errno != EINTR && errno != EAGAIN) {
          goto cleanup;
        }
      }
      if (fds[2 + i].revents) {
        n = read(streams[i].fd, streams[i].bytes,
                 sizeof(streams[i].bytes));
        if (n > 0) {
          streams[i].offset = 0;
          streams[i].size = (size_t) n;
        } else if (n == 0) {
          close(streams[i].fd);
          streams[i].fd = -1;
        } else if (errno != EINTR && errno != EAGAIN) {
          goto cleanup;
        }
      }
    }
  }

  /* Keep the waitable identity through the final group signal, then reap.
   * Output backpressure must never extend a numeric PID past ownership. */
  observed = lso_time_finish(child, exited, &status);
  child = -1;
  if (observed < 0) {
    goto cleanup;
  }
  end = lso_time_now();
  if (end < start) {
    goto cleanup;
  }
  result = timed_out ? 124 :
      (lso_stop_signal ? 128 + (int) lso_stop_signal :
       (WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status)));
  /* stderr is metadata only; no TAP result or TAP parsing is added. */
  count = snprintf(marker, sizeof(marker),
                   "\n# child-time: 1 %.9f ms %d %d %s\n",
                   (end - start) * 1000.0, result, timed_out, name);
  if (count < 0 || (size_t) count >= sizeof(marker)) {
    result = 125;
    goto cleanup;
  }
  sent = 0;
  while (sent < (size_t) count) {
    n = write(STDERR_FILENO, marker + sent, (size_t) count - sent);
    if (n > 0) {
      sent += (size_t) n;
      continue;
    }
    now = lso_time_now();
    if (now < end || now >= end + 0.1 ||
        (n < 0 && errno != EAGAIN && errno != EINTR)) {
      result = 125;
      goto cleanup;
    }
    fds[0].fd = STDERR_FILENO;
    fds[0].events = POLLOUT;
    if (poll(fds, 1, 1) < 0 && errno != EINTR) {
      result = 125;
      goto cleanup;
    }
  }

cleanup:
  /* No success record is emitted on setup, clock or output errors. All
   * failures converge here, including EOF of the outer executor's lifeline. */
  if (child > 0) {
    (void) lso_time_finish(child, exited, &status);
  }
  lso_wake_fd = -1;
  for (i = 0; i < 2; ++i) {
    if (streams[i].fd >= 0) {
      close(streams[i].fd);
    }
    if (streams[i].flags >= 0) {
      (void) fcntl(i + 1, F_SETFL, streams[i].flags);
    }
    if (pipes[i][0] >= 0) {
      close(pipes[i][0]);
    }
    if (pipes[i][1] >= 0) {
      close(pipes[i][1]);
    }
    if (wake[i] >= 0) {
      close(wake[i]);
    }
  }
  close(control);
  return result;
}

static int
lso_time_execute(double duration, const char *name, char **command)
{
  double start;
  int control[2];
  int status;
  int result;
  pid_t guardian;
  pid_t waited;
  sigset_t blocked;
  sigset_t original;
  struct sigaction action;

  /* C0 is before every pipe, supervisor fork and command spawn. */
  start = lso_time_now();
  if (start < 0.0 || fcntl(STDOUT_FILENO, F_GETFL) < 0 ||
      fcntl(STDERR_FILENO, F_GETFL) < 0) {
    return 125;
  }
  sigemptyset(&blocked);
  sigaddset(&blocked, SIGTERM);
  sigaddset(&blocked, SIGINT);
  sigaddset(&blocked, SIGHUP);
  sigaddset(&blocked, SIGCHLD);
  if (sigprocmask(SIG_BLOCK, &blocked, &original) < 0) {
    return 125;
  }
  if (lso_time_pipe(control) < 0) {
    return 125;
  }
  if (lso_time_nonblock(control[1]) < 0) {
    close(control[0]);
    close(control[1]);
    return 125;
  }
  memset(&action, 0, sizeof(action));
  sigemptyset(&action.sa_mask);
  action.sa_handler = SIG_IGN;
  if (sigaction(SIGPIPE, &action, NULL) < 0) {
    close(control[0]);
    close(control[1]);
    return 125;
  }
  guardian = fork();
  if (guardian == 0) {
    close(control[1]);
    result = lso_time_guard(control[0], start, duration, name,
                            command, &original);
    _exit(result);
  }
  close(control[0]);
  if (guardian < 0) {
    close(control[1]);
    return 125;
  }
  lso_wake_fd = control[1];
  action.sa_handler = lso_time_signal;
  if (sigaction(SIGTERM, &action, NULL) < 0 ||
      sigaction(SIGINT, &action, NULL) < 0 ||
      sigaction(SIGHUP, &action, NULL) < 0 ||
      sigprocmask(SIG_SETMASK, &original, NULL) < 0) {
    close(control[1]);
    lso_time_reap(guardian);
    return 125;
  }
  do {
    waited = waitpid(guardian, &status, 0);
  } while (waited < 0 && errno == EINTR);
  lso_wake_fd = -1;
  close(control[1]);
  if (waited != guardian) {
    return 125;
  }
  return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
}
#endif /* HAVE_LSO_CHILD_TIME */

static int
lso_child_time(int argc, char **argv)
{
  double duration;
  char *end;
  const unsigned char *p;

  /* This grammar is deliberately separate from the legacy timeout CLI. */
  if (argc < 6 || strcmp(argv[4], "--") != 0 || !argv[2][0] ||
      strlen(argv[2]) > 4096 || !argv[5][0]) {
    goto invalid;
  }
  for (p = (const unsigned char *) argv[2]; *p; ++p) {
    if (*p < 32 || *p == 127) {
      goto invalid;
    }
  }
  p = (const unsigned char *) argv[3];
  if (*p < '0' || *p > '9') {
    goto invalid;
  }
  while (*p >= '0' && *p <= '9') {
    ++p;
  }
  if (*p == '.') {
    ++p;
    if (*p < '0' || *p > '9') {
      goto invalid;
    }
    while (*p >= '0' && *p <= '9') {
      ++p;
    }
  }
  errno = 0;
  duration = strtod(argv[3], &end);
  if (*p || *end || errno || !isfinite(duration) ||
      duration < 0.0 || duration > 86400.0) {
    goto invalid;
  }
#if defined(HAVE_LSO_CHILD_TIME)
  if (duration == 0.0 &&
      (isatty(STDIN_FILENO) || isatty(STDOUT_FILENO) ||
       isatty(STDERR_FILENO))) {
    /* Meson interactive execution inherits the terminal. Preserve that
     * command's descriptors/session and let its outer harness own signals.
     * Interactive Meson does not capture this diagnostic in its JSON log. */
    fprintf(stderr, "\n# child-time: 1 unmeasured terminal %s\n", argv[2]);
    execvp(argv[5], &argv[5]);
    return errno == ENOENT ? 127 : 126;
  }
  return lso_time_execute(duration, argv[2], &argv[5]);
#else
  fprintf(stderr, "lso-timeout: child-time unsupported; use outer-only\n");
  return 125;
#endif

invalid:
  fprintf(stderr, "Usage: lso-timeout --record-child-time NAME "
          "SECONDS -- COMMAND...\n");
  return 125;
}
#endif /* LSO_CHILD_TIME_H */
