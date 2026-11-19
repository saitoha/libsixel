/* SPDX-License-Identifier: MIT */
/* Verify that the executable's TLS segment is available at run time. */
#if defined(__PCC__) || defined(__TINYC__)
# error No supported TLS storage class
#elif defined(_MSC_VER)
# define PROBE_TLS __declspec(thread)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
# define PROBE_TLS _Thread_local
#elif defined(__GNUC__) || defined(__clang__)
# define PROBE_TLS __thread
#else
# error No supported TLS storage class
#endif

static PROBE_TLS volatile unsigned int probe_value = 3;

int
main(void)
{
    if (probe_value != 3) {
        return 1;
    }
    probe_value = 5;
    return probe_value != 5;
}
