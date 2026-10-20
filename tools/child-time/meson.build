# The executor is a build-machine tool, never a cross-target executable.
# A missing native compiler/capability retains the real command and raw outer
# measurement. This is not a capability skip of the test itself.
child_time_executor = disabler()
have_child_time_executor = false
child_time_fallback = 'outer-only: native POSIX executor unavailable'
if add_languages('c', native: true, required: false)
  child_time_cc = meson.get_compiler('c', native: true)
  child_time_clock = child_time_cc.find_library('rt', required: false)
  child_time_probe = '''
#if defined(__COSMOPOLITAN__) || defined(__EMSCRIPTEN__) || \
    defined(__wasm__) || defined(__CYGWIN__)
#error The timing supervisor requires a native POSIX process executable.
#endif
#define _POSIX_C_SOURCE 200809L
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <poll.h>
#include <time.h>
int main(void) {
  struct timespec t;
  struct pollfd p;
  struct sigaction a;
  sigset_t s;
  siginfo_t info;
  int f[2];
  sigemptyset(&s);
  sigaction(SIGCHLD, &a, 0);
  sigprocmask(SIG_BLOCK, &s, 0);
  pipe(f);
  fcntl(f[0], F_SETFL, O_NONBLOCK);
  poll(&p, 0, 0);
  setsid();
  setpgid(0, 0);
  kill(0, 0);
  waitpid(-1, 0, WNOHANG);
  waitid(P_PID, 1, &info, WEXITED | WNOWAIT | WNOHANG);
  fork();
  return clock_gettime(CLOCK_MONOTONIC, &t);
}
'''
  if child_time_cc.links(child_time_probe, dependencies: child_time_clock,
      name: 'native complete-command timing primitives')
    child_time_config = configuration_data()
    foreach feature : ['FORK', 'WAITPID', 'SYS_WAIT_H', 'TIME_H', 'ERRNO_H',
                       'SIGNAL_H', 'SYS_TYPES_H', 'SYS_TIME_H', 'UNISTD_H',
                       'NANOSLEEP', 'LSO_CHILD_TIME']
      child_time_config.set('HAVE_' + feature, 1)
    endforeach
    child_time_header = configure_file(output: 'lso-timeout-native.h',
      configuration: child_time_config)
    child_time_executor = executable('lso-timeout', '../lso-timeout.c',
      child_time_header, native: true,
      c_args: ['-DLSO_TIMEOUT_NATIVE=1'],
      dependencies: child_time_clock,
      include_directories: include_directories('.'),
      install: false)
    have_child_time_executor = true
  endif
endif
message(have_child_time_executor ?
  'Test timing: raw Meson outer + complete-command child (non-additive)' :
  'Test timing: ' + child_time_fallback)
