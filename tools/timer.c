/*
 * timer.c - portable high-resolution timestamp helper
 *
 * This utility prints the current time as seconds.nanoseconds with
 * millisecond-or-better precision. It is intended as a fallback when
 * "date +%s.%N" is unavailable or lacks sub-second resolution.
 */

#if !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#if defined(HAVE_TIME_H)
#include <time.h>
#endif

#ifdef _WIN32
#include <windows.h>
#elif defined(HAVE_SYS_TIME_H)
#include <sys/time.h>
#endif

static double
get_time_seconds(void)
{
#ifdef _WIN32
  LARGE_INTEGER freq;
  LARGE_INTEGER counter;

  if (!QueryPerformanceFrequency(&freq)) {
    return -1.0;
  }
  if (!QueryPerformanceCounter(&counter)) {
    return -1.0;
  }

  return (double) counter.QuadPart / (double) freq.QuadPart;
#elif defined(HAVE_CLOCK_GETTIME) && defined(CLOCK_REALTIME)
  struct timespec ts;

  if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
    return -1.0;
  }

  return (double) ts.tv_sec + (double) ts.tv_nsec / 1.0e9;
#elif defined(HAVE_GETTIMEOFDAY)
  struct timeval tv;

  if (gettimeofday(&tv, NULL) != 0) {
    return -1.0;
  }

  return (double) tv.tv_sec + (double) tv.tv_usec / 1.0e6;
#else
  return -1.0;
#endif
}

int
main(void)
{
  double now;

  now = get_time_seconds();
  if (now < 0.0) {
    return EXIT_FAILURE;
  }

  if (printf("%.9f\n", now) < 0) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
