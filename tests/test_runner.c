/*
 * Unified test runner for C-based tests under tests/.
 *
 * The runner dispatches to per-test functions so multiple test sources can be
 * linked into a single executable while keeping per-test exit status.
 */

#if !defined(_POSIX_C_SOURCE)
# define _POSIX_C_SOURCE 200809L
#endif
#if !defined(_XOPEN_SOURCE)
/*
 * NetBSD may keep getpgid() hidden under strict POSIX mode unless the XSI
 * surface is also requested. Keep this aligned with POSIX.1-2008 visibility.
 */
# define _XOPEN_SOURCE 700
#endif

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#if HAVE_TIME_H
# include <time.h>
#endif

#if !defined(_WIN32)
# if HAVE_FCNTL_H
#  include <fcntl.h>
# endif
# if HAVE_SIGNAL_H
#  include <signal.h>
# endif
# if HAVE_UNISTD_H
#  include <unistd.h>
# endif
# if HAVE_SYS_TYPES_H
#  include <sys/types.h>
# endif
# if HAVE_SYS_WAIT_H
#  include <sys/wait.h>
# endif
#endif

#if defined(_WIN32) || defined(__CYGWIN__)
# include <windows.h>
#endif

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__) \
    && HAVE_FCNTL_H && HAVE_FORK && HAVE_NANOSLEEP \
    && HAVE_SIGNAL_H && HAVE_SYS_WAIT_H && HAVE_UNISTD_H \
    && HAVE_WAITPID && HAVE_EXECVP
# define TEST_RUNNER_HAVE_POSIX_SIGINT_RUNNER 1
#else
# define TEST_RUNNER_HAVE_POSIX_SIGINT_RUNNER 0
#endif

static int
test_runner_setenv_portable(char const *name, char const *value);

#if TEST_RUNNER_HAVE_POSIX_SIGINT_RUNNER
# define TEST_RUNNER_SIGINT_SYNC_FD_ENVVAR \
    "LSO_TEST_SIGINT_NOTIFY_FD"
#endif

#if TEST_RUNNER_HAVE_POSIX_SIGINT_RUNNER
# define TEST_RUNNER_SIGINT_SYNC_PID_ENVVAR \
    "LSO_TEST_SIGINT_NOTIFY_PID"
# define TEST_RUNNER_SIGINT_SYNC_EVENT_ENVVAR \
    "LSO_TEST_SIGINT_NOTIFY_EVENT"
# define TEST_RUNNER_SIGINT_SYNC_EVENT_DISPATCH_START \
    "callback_dispatch_start"

static volatile sig_atomic_t test_runner_sigint_sync_ready;

static void
test_runner_sigint_sync_handler(int signum)
{
    (void)signum;
    test_runner_sigint_sync_ready = 1;
}
#endif

int test_filter_0001_filter_clip(int argc, char **argv);
int test_color_0001_din99d_forward(int argc, char **argv);
int test_color_0002_din99d_inverse(int argc, char **argv);
int test_color_0003_din99d_byte(int argc, char **argv);
int test_color_0004(int argc, char **argv);
int test_color_0005(int argc, char **argv);
int test_color_0006(int argc, char **argv);
int test_color_0007(int argc, char **argv);
int test_color_0008(int argc, char **argv);
int test_color_0009(int argc, char **argv);
int test_color_0010(int argc, char **argv);
int test_color_0011(int argc, char **argv);
int test_qa_0001(int argc, char **argv);
int test_qa_0002(int argc, char **argv);
int test_qa_0003(int argc, char **argv);
int test_qa_0004(int argc, char **argv);
int test_qa_0005(int argc, char **argv);
int test_qa_0006(int argc, char **argv);
int test_qa_0007(int argc, char **argv);
int test_qa_0008(int argc, char **argv);
int test_qa_0009(int argc, char **argv);
int test_qa_0010(int argc, char **argv);
int test_qa_0011(int argc, char **argv);
int test_qa_0012(int argc, char **argv);
int test_qa_0013(int argc, char **argv);
int test_qa_0017(int argc, char **argv);
int test_image_values(int argc, char **argv);
int test_image_sample_range(int argc, char **argv);
int test_image_dimensions(int argc, char **argv);
int test_named_output_0037(int argc, char **argv);
int test_named_output_0038(int argc, char **argv);
int test_named_output_0039(int argc, char **argv);
int test_named_output_0040(int argc, char **argv);
int test_named_output_0001(int argc, char **argv);
int test_named_output_0002(int argc, char **argv);
int test_named_output_0003(int argc, char **argv);
int test_named_output_0004(int argc, char **argv);
int test_named_output_0005(int argc, char **argv);
int test_named_output_0006(int argc, char **argv);
int test_named_output_0007(int argc, char **argv);
int test_named_output_0008(int argc, char **argv);
int test_named_output_0009(int argc, char **argv);
int test_named_output_0010(int argc, char **argv);
int test_named_output_0011(int argc, char **argv);
int test_named_output_0012(int argc, char **argv);
int test_named_output_0013(int argc, char **argv);
int test_named_output_0014(int argc, char **argv);
int test_named_output_0015(int argc, char **argv);
int test_named_output_0016(int argc, char **argv);
int test_named_output_0017(int argc, char **argv);
int test_named_output_0018(int argc, char **argv);
int test_named_output_0019(int argc, char **argv);
int test_named_output_0020(int argc, char **argv);
int test_named_output_0021(int argc, char **argv);
int test_named_output_0022(int argc, char **argv);
int test_named_output_0023(int argc, char **argv);
int test_named_output_0024(int argc, char **argv);
int test_named_output_0025(int argc, char **argv);
int test_named_output_0026(int argc, char **argv);
int test_named_output_0027(int argc, char **argv);
int test_named_output_0028(int argc, char **argv);
int test_named_output_0029(int argc, char **argv);
int test_named_output_0030(int argc, char **argv);
int test_named_output_0031(int argc, char **argv);
int test_named_output_0032(int argc, char **argv);
int test_named_output_0033(int argc, char **argv);
int test_named_output_0034(int argc, char **argv);
int test_png_color_0030(int argc, char **argv);
int test_png_color_0031(int argc, char **argv);
int test_png_color_0026(int argc, char **argv);
int test_png_color_0027(int argc, char **argv);
int test_png_color_0028(int argc, char **argv);
int test_png_color_0029(int argc, char **argv);
int test_cdef_0001(int argc, char **argv);
int test_cdef_0002(int argc, char **argv);
int test_cdef_0003(int argc, char **argv);
int test_cdef_0004(int argc, char **argv);
int test_cdef_0005(int argc, char **argv);
int test_cdef_0006(int argc, char **argv);
int test_cdef_0007(int argc, char **argv);
int test_icc_0009(int argc, char **argv);
int test_icc_0010(int argc, char **argv);
int test_icc_0011(int argc, char **argv);
int test_icc_0012(int argc, char **argv);
int test_icc_0013(int argc, char **argv);
int test_icc_0014(int argc, char **argv);
int test_icc_0015(int argc, char **argv);
int test_icc_0016(int argc, char **argv);
int test_icc_0017(int argc, char **argv);
int test_color_0012(int argc, char **argv);
int test_color_0013(int argc, char **argv);
int test_color_public_float(int argc, char **argv);
int test_color_public_float_fault(int argc, char **argv);
int test_png_key_0001(int argc, char **argv);
int test_png_key_0002(int argc, char **argv);
int test_cdef_0008(int argc, char **argv);
int test_cdef_0009(int argc, char **argv);
int test_cdef_0010(int argc, char **argv);
int test_cdef_0011(int argc, char **argv);
int test_cdef_0012(int argc, char **argv);
int test_cdef_0013(int argc, char **argv);
int test_cms_0014(int argc, char **argv);
int test_cms_0015(int argc, char **argv);
int test_cms_0016(int argc, char **argv);
int test_cms_0017(int argc, char **argv);
int test_cms_0018(int argc, char **argv);
int test_cms_0019(int argc, char **argv);
int test_cms_0020(int argc, char **argv);
int test_cms_0021(int argc, char **argv);
int test_cms_0024(int argc, char **argv);
int test_cms_0025(int argc, char **argv);
int test_cms_0033(int argc, char **argv);
int test_cms_0044(int argc, char **argv);
int test_cms_0045(int argc, char **argv);
int test_cms_0053(int argc, char **argv);
int test_cms_0128(int argc, char **argv);
int test_cms_0129(int argc, char **argv);
int test_cms_0130(int argc, char **argv);
int test_cms_0146(int argc, char **argv);
int test_cms_0147(int argc, char **argv);
int test_cms_0181(int argc, char **argv);
int test_cms_0195(int argc, char **argv);
int test_pmap_0053(int argc, char **argv);
int test_filter_0002_filter_sample(int argc, char **argv);
int test_filter_0003_filter_resize(int argc, char **argv);
int test_filter_0004_filter_colors(int argc, char **argv);
int test_filter_0005_filter_lookup(int argc, char **argv);
int test_filter_0006_filter_final_merge(int argc, char **argv);
int test_filter_0007_filter_fhedt(int argc, char **argv);
int test_filter_0008_filter_load(int argc, char **argv);
int test_filter_0009_filter_dither(int argc, char **argv);
int test_filter_0010_filter_encode(int argc, char **argv);
int test_filter_0011_filter_encode_accumulation_buffer(int argc, char **argv);
int test_filter_0012_filter_encode_6delta_plane(int argc, char **argv);
int test_filter_0013_filter_encode_6delta_keycolor(int argc, char **argv);
int test_filter_0014_filter_sample_solid(int argc, char **argv);
int test_clamp_0001(int argc, char **argv);
int test_clamp_0002(int argc, char **argv);
int test_clamp_0003(int argc, char **argv);
int test_clamp_0004(int argc, char **argv);
int test_clamp_0005(int argc, char **argv);
int test_clamp_0006(int argc, char **argv);
int test_clamp_0007(int argc, char **argv);
int test_clamp_0008(int argc, char **argv);
int test_clamp_0009(int argc, char **argv);
int test_clamp_0010(int argc, char **argv);
int test_clamp_0011(int argc, char **argv);
int test_clamp_0012(int argc, char **argv);
int test_clamp_0013(int argc, char **argv);
int test_clamp_0014(int argc, char **argv);
int test_clamp_0015(int argc, char **argv);
int test_clamp_0016(int argc, char **argv);
int test_clamp_0017(int argc, char **argv);
int test_clamp_0018(int argc, char **argv);
int test_clamp_0019(int argc, char **argv);
int test_clamp_0020(int argc, char **argv);
int test_clamp_0021(int argc, char **argv);
int test_clamp_0022(int argc, char **argv);
int test_clamp_0023(int argc, char **argv);
int test_clamp_0024(int argc, char **argv);
int test_clamp_0025(int argc, char **argv);
int test_clamp_0026(int argc, char **argv);
int test_clamp_0027(int argc, char **argv);
int test_clamp_0028(int argc, char **argv);
int test_clamp_0029(int argc, char **argv);
int test_clamp_0030(int argc, char **argv);
int test_clamp_0031(int argc, char **argv);
int test_clamp_0033(int argc, char **argv);
int test_clamp_0034(int argc, char **argv);
int test_clamp_0035(int argc, char **argv);
int test_clamp_0036(int argc, char **argv);
int test_clamp_0037(int argc, char **argv);
int test_clamp_0038(int argc, char **argv);
int test_clamp_0039(int argc, char **argv);
int test_clamp_0040(int argc, char **argv);
int test_clamp_0041(int argc, char **argv);
int test_clamp_0042(int argc, char **argv);
int test_clamp_0043(int argc, char **argv);
int test_or_native_0030(int argc, char **argv);
int test_or_native_0031(int argc, char **argv);
int test_or_native_0032(int argc, char **argv);
int test_or_native_0033(int argc, char **argv);
int test_or_native_0034(int argc, char **argv);
int test_or_native_0035(int argc, char **argv);
int test_or_native_0036(int argc, char **argv);
int test_or_native_0037(int argc, char **argv);
int test_or_native_0038(int argc, char **argv);
int test_or_native_0039(int argc, char **argv);
int test_or_native_0040(int argc, char **argv);
int test_or_native_0041(int argc, char **argv);
int test_macro_0009(int argc, char **argv);
int test_macro_0010(int argc, char **argv);
int test_macro_0011(int argc, char **argv);
int test_macro_0012(int argc, char **argv);
int test_macro_0013(int argc, char **argv);
int test_macro_0014(int argc, char **argv);
int test_macro_0015(int argc, char **argv);
int test_macro_0016(int argc, char **argv);
int test_macro_0017(int argc, char **argv);
int test_macro_0018(int argc, char **argv);
int test_macro_0019(int argc, char **argv);
int test_macro_0020(int argc, char **argv);
int test_macro_0021(int argc, char **argv);
int test_macro_0022(int argc, char **argv);
int test_macro_0023(int argc, char **argv);
int test_macro_0024(int argc, char **argv);
int test_macro_0025(int argc, char **argv);
int test_hls_native_0014(int argc, char **argv);
int test_hls_native_0015(int argc, char **argv);
int test_delta_native_0048(int argc, char **argv);
int test_delta_native_0049(int argc, char **argv);
int test_delta_native_0050(int argc, char **argv);
int test_delta_native_0051(int argc, char **argv);
int test_delta_native_0052(int argc, char **argv);
int test_delta_native_0053(int argc, char **argv);
int test_delta_native_0054(int argc, char **argv);
int test_delta_native_0055(int argc, char **argv);
int test_delta_native_0056(int argc, char **argv);
int test_delta_native_0057(int argc, char **argv);
int test_delta_native_0058(int argc, char **argv);
int test_delta_native_0059(int argc, char **argv);
int test_delta_native_0060(int argc, char **argv);
int test_delta_native_0061(int argc, char **argv);
int test_delta_native_0062(int argc, char **argv);
int test_delta_native_0063(int argc, char **argv);
int test_delta_native_0064(int argc, char **argv);
int test_delta_native_0065(int argc, char **argv);
int test_delta_native_0066(int argc, char **argv);
int test_delta_native_0067(int argc, char **argv);
int test_delta_native_0068(int argc, char **argv);
int test_delta_native_0069(int argc, char **argv);
int test_delta_native_0070(int argc, char **argv);
int test_delta_native_0071(int argc, char **argv);
int test_delta_native_0072(int argc, char **argv);
int test_delta_native_0073(int argc, char **argv);
int test_delta_native_0074(int argc, char **argv);
int test_delta_native_0075(int argc, char **argv);
int test_delta_native_0076(int argc, char **argv);
int test_delta_native_0077(int argc, char **argv);
int test_delta_native_0078(int argc, char **argv);
int test_delta_native_0079(int argc, char **argv);
int test_delta_native_0080(int argc, char **argv);
int test_delta_native_0119(int argc, char **argv);
int test_delta_native_0120(int argc, char **argv);
int test_delta_native_0121(int argc, char **argv);
int test_delta_native_0122(int argc, char **argv);
int test_delta_native_0123(int argc, char **argv);
int test_delta_native_0124(int argc, char **argv);
int test_delta_native_0125(int argc, char **argv);
int test_delta_native_0126(int argc, char **argv);
int test_delta_native_0127(int argc, char **argv);
int test_delta_native_0128(int argc, char **argv);
int test_delta_native_0129(int argc, char **argv);
int test_delta_native_0130(int argc, char **argv);
int test_delta_native_0131(int argc, char **argv);
int test_delta_native_0132(int argc, char **argv);
int test_delta_native_0133(int argc, char **argv);
int test_delta_native_0134(int argc, char **argv);
int test_delta_native_0135(int argc, char **argv);
int test_delta_native_0136(int argc, char **argv);
int test_delta_native_0137(int argc, char **argv);
int test_delta_native_0138(int argc, char **argv);
int test_delta_native_0139(int argc, char **argv);
int test_delta_native_0140(int argc, char **argv);
int test_delta_native_0141(int argc, char **argv);
int test_delta_native_0142(int argc, char **argv);
int test_delta_native_0143(int argc, char **argv);
int test_delta_native_0144(int argc, char **argv);
int test_delta_native_0145(int argc, char **argv);
int test_delta_native_0146(int argc, char **argv);
int test_delta_native_0147(int argc, char **argv);
int test_delta_native_0148(int argc, char **argv);
int test_delta_native_0149(int argc, char **argv);
int test_delta_native_0150(int argc, char **argv);
int test_delta_native_0151(int argc, char **argv);
int test_delta_native_0152(int argc, char **argv);
int test_delta_native_0153(int argc, char **argv);
int test_delta_native_0154(int argc, char **argv);
int test_delta_native_0155(int argc, char **argv);
int test_delta_native_0156(int argc, char **argv);
int test_delta_native_0157(int argc, char **argv);
int test_delta_native_0158(int argc, char **argv);
int test_delta_native_0159(int argc, char **argv);
int test_delta_native_0160(int argc, char **argv);
int test_delta_native_0161(int argc, char **argv);
int test_delta_native_0162(int argc, char **argv);
int test_delta_native_0163(int argc, char **argv);
int test_delta_native_0164(int argc, char **argv);
int test_delta_native_0165(int argc, char **argv);
int test_delta_native_0166(int argc, char **argv);
int test_delta_native_0167(int argc, char **argv);
int test_delta_native_0168(int argc, char **argv);
int test_delta_native_0169(int argc, char **argv);
int test_delta_native_0170(int argc, char **argv);
int test_delta_native_0171(int argc, char **argv);
int test_delta_native_0172(int argc, char **argv);
int test_delta_native_0173(int argc, char **argv);
int test_delta_native_0174(int argc, char **argv);
int test_delta_native_0175(int argc, char **argv);
int test_delta_native_0176(int argc, char **argv);
int test_delta_native_0177(int argc, char **argv);
int test_delta_native_0178(int argc, char **argv);
int test_delta_native_0179(int argc, char **argv);
int test_delta_native_0180(int argc, char **argv);
int test_delta_native_0181(int argc, char **argv);
int test_delta_native_0182(int argc, char **argv);
int test_delta_native_0183(int argc, char **argv);
int test_delta_native_0184(int argc, char **argv);
int test_delta_native_0185(int argc, char **argv);
int test_delta_native_0186(int argc, char **argv);
int test_delta_native_0187(int argc, char **argv);
int test_delta_native_0188(int argc, char **argv);
int test_delta_native_0189(int argc, char **argv);
int test_delta_native_0190(int argc, char **argv);
int test_delta_native_0191(int argc, char **argv);
int test_delta_native_0192(int argc, char **argv);
int test_delta_native_0193(int argc, char **argv);
int test_delta_native_0194(int argc, char **argv);
int test_delta_native_0195(int argc, char **argv);
int test_delta_native_0196(int argc, char **argv);
int test_delta_native_0197(int argc, char **argv);
int test_delta_native_0198(int argc, char **argv);
int test_delta_native_0199(int argc, char **argv);
int test_delta_native_0200(int argc, char **argv);
int test_delta_native_0201(int argc, char **argv);
int test_delta_native_0202(int argc, char **argv);
int test_delta_native_0203(int argc, char **argv);
int test_delta_native_0204(int argc, char **argv);
int test_delta_native_0205(int argc, char **argv);
int test_delta_native_0206(int argc, char **argv);
int test_delta_native_0207(int argc, char **argv);
int test_delta_native_0208(int argc, char **argv);
int test_delta_native_0209(int argc, char **argv);
int test_delta_native_0210(int argc, char **argv);
int test_delta_native_0211(int argc, char **argv);
int test_delta_native_0212(int argc, char **argv);
int test_delta_native_0213(int argc, char **argv);
int test_delta_native_0214(int argc, char **argv);
int test_delta_native_0215(int argc, char **argv);
int test_delta_native_0216(int argc, char **argv);
int test_delta_native_0217(int argc, char **argv);
int test_delta_native_0218(int argc, char **argv);
int test_delta_native_0219(int argc, char **argv);
int test_delta_native_0220(int argc, char **argv);
int test_delta_native_0221(int argc, char **argv);
int test_delta_native_0222(int argc, char **argv);
int test_delta_native_0223(int argc, char **argv);
int test_delta_native_0224(int argc, char **argv);
int test_delta_native_0225(int argc, char **argv);
int test_delta_native_0226(int argc, char **argv);
int test_delta_native_0227(int argc, char **argv);
int test_delta_native_0228(int argc, char **argv);
int test_delta_native_0229(int argc, char **argv);
int test_delta_native_0230(int argc, char **argv);
int test_delta_native_0231(int argc, char **argv);
int test_delta_native_0232(int argc, char **argv);
int test_delta_native_0233(int argc, char **argv);
int test_delta_native_0234(int argc, char **argv);
int test_delta_native_0235(int argc, char **argv);
int test_delta_native_0236(int argc, char **argv);
int test_delta_native_0237(int argc, char **argv);
int test_delta_native_0238(int argc, char **argv);
int test_delta_native_0239(int argc, char **argv);
int test_delta_native_0101(int argc, char **argv);
int test_delta_native_0102(int argc, char **argv);
int test_delta_native_0103(int argc, char **argv);
int test_delta_native_0104(int argc, char **argv);
int test_delta_native_0105(int argc, char **argv);
int test_delta_native_0106(int argc, char **argv);
int test_delta_native_0107(int argc, char **argv);
int test_delta_native_0108(int argc, char **argv);
int test_delta_native_0109(int argc, char **argv);
int test_delta_native_0110(int argc, char **argv);
int test_delta_native_0111(int argc, char **argv);
int test_delta_native_0112(int argc, char **argv);
int test_delta_native_0113(int argc, char **argv);
int test_delta_native_0114(int argc, char **argv);
int test_delta_native_0115(int argc, char **argv);
int test_delta_native_0116(int argc, char **argv);
int test_delta_native_0117(int argc, char **argv);
int test_delta_native_0118(int argc, char **argv);
int test_delta_native_0081(int argc, char **argv);
int test_delta_native_0082(int argc, char **argv);
int test_delta_native_0083(int argc, char **argv);
int test_delta_native_0084(int argc, char **argv);
int test_delta_native_0085(int argc, char **argv);
int test_delta_native_0086(int argc, char **argv);
int test_delta_native_0087(int argc, char **argv);
int test_delta_native_0088(int argc, char **argv);
int test_delta_native_0089(int argc, char **argv);
int test_delta_native_0090(int argc, char **argv);
int test_delta_native_0091(int argc, char **argv);
int test_delta_native_0092(int argc, char **argv);
int test_delta_native_0093(int argc, char **argv);
int test_delta_native_0094(int argc, char **argv);
int test_delta_native_0095(int argc, char **argv);
int test_delta_native_0096(int argc, char **argv);
int test_delta_native_0097(int argc, char **argv);
int test_delta_native_0098(int argc, char **argv);
int test_delta_native_0099(int argc, char **argv);
int test_delta_native_0100(int argc, char **argv);
int test_delta_native_0033(int argc, char **argv);
int test_delta_native_0034(int argc, char **argv);
int test_delta_native_0035(int argc, char **argv);
int test_delta_native_0036(int argc, char **argv);
int test_delta_native_0037(int argc, char **argv);
int test_delta_native_0038(int argc, char **argv);
int test_delta_native_0039(int argc, char **argv);
int test_delta_native_0040(int argc, char **argv);
int test_delta_native_0041(int argc, char **argv);
int test_delta_native_0042(int argc, char **argv);
int test_delta_native_0043(int argc, char **argv);
int test_delta_native_0044(int argc, char **argv);
int test_delta_native_0045(int argc, char **argv);
int test_delta_native_0046(int argc, char **argv);
int test_delta_native_0047(int argc, char **argv);
int test_delta_native_0018(int argc, char **argv);
int test_delta_native_0019(int argc, char **argv);
int test_delta_native_0020(int argc, char **argv);
int test_delta_native_0021(int argc, char **argv);
int test_delta_native_0022(int argc, char **argv);
int test_delta_native_0023(int argc, char **argv);
int test_delta_native_0024(int argc, char **argv);
int test_delta_native_0025(int argc, char **argv);
int test_delta_native_0026(int argc, char **argv);
int test_delta_native_0027(int argc, char **argv);
int test_delta_native_0028(int argc, char **argv);
int test_delta_native_0029(int argc, char **argv);
int test_delta_native_0030(int argc, char **argv);
int test_delta_native_0031(int argc, char **argv);
int test_delta_native_0032(int argc, char **argv);
int test_delta_native_0015(int argc, char **argv);
int test_delta_native_0016(int argc, char **argv);
int test_delta_native_0017(int argc, char **argv);
int test_delta_native_0001(int argc, char **argv);
int test_delta_native_0002(int argc, char **argv);
int test_delta_native_0003(int argc, char **argv);
int test_delta_native_0004(int argc, char **argv);
int test_delta_native_0005(int argc, char **argv);
int test_delta_native_0006(int argc, char **argv);
int test_delta_native_0007(int argc, char **argv);
int test_delta_native_0008(int argc, char **argv);
int test_delta_native_0009(int argc, char **argv);
int test_delta_native_0010(int argc, char **argv);
int test_delta_native_0011(int argc, char **argv);
int test_delta_native_0012(int argc, char **argv);
int test_delta_native_0013(int argc, char **argv);
int test_delta_native_0014(int argc, char **argv);
int test_export_capture_0001(int argc, char **argv);
int test_export_capture_0002(int argc, char **argv);
int test_export_capture_0003(int argc, char **argv);
int test_export_capture_0004(int argc, char **argv);
int test_export_capture_0005(int argc, char **argv);
int test_export_capture_0006(int argc, char **argv);
int test_png_snapshot_0010(int argc, char **argv);
int test_png_snapshot_0011(int argc, char **argv);
int test_png_snapshot_0012(int argc, char **argv);
int test_png_snapshot_0013(int argc, char **argv);
int test_png_snapshot_0014(int argc, char **argv);
int test_png_snapshot_0015(int argc, char **argv);
int test_png_snapshot_0016(int argc, char **argv);
int test_png_snapshot_0017(int argc, char **argv);
int test_png_snapshot_0018(int argc, char **argv);
int test_png_snapshot_0019(int argc, char **argv);
int test_png_snapshot_0020(int argc, char **argv);
int test_png_snapshot_0021(int argc, char **argv);
int test_png_snapshot_0022(int argc, char **argv);
int test_png_snapshot_0023(int argc, char **argv);
int test_png_snapshot_0030(int argc, char **argv);
int test_png_snapshot_0031(int argc, char **argv);
int test_png_snapshot_0029(int argc, char **argv);
int test_png_snapshot_0028(int argc, char **argv);
int test_png_snapshot_0027(int argc, char **argv);
int test_png_snapshot_0026(int argc, char **argv);
int test_png_snapshot_0025(int argc, char **argv);
int test_png_snapshot_0024(int argc, char **argv);
int test_highcolor_0001(int argc, char **argv);
int test_highcolor_0002(int argc, char **argv);
int test_highcolor_0003(int argc, char **argv);
int test_highcolor_0004(int argc, char **argv);
int test_highcolor_0005(int argc, char **argv);
int test_highcolor_0006(int argc, char **argv);
int test_highcolor_0007(int argc, char **argv);
int test_highcolor_0008(int argc, char **argv);
int test_highcolor_0009(int argc, char **argv);
int test_highcolor_0010(int argc, char **argv);
int test_highcolor_0011(int argc, char **argv);
int test_highcolor_0012(int argc, char **argv);
int test_highcolor_0013(int argc, char **argv);
int test_highcolor_0014(int argc, char **argv);
int test_highcolor_0015(int argc, char **argv);
int test_highcolor_0016(int argc, char **argv);
int test_highcolor_0017(int argc, char **argv);
int test_highcolor_0018(int argc, char **argv);
int test_highcolor_0019(int argc, char **argv);
int test_highcolor_0020(int argc, char **argv);
int test_highcolor_0021(int argc, char **argv);
int test_highcolor_0022(int argc, char **argv);
int test_highcolor_0023(int argc, char **argv);
int test_highcolor_0024(int argc, char **argv);
int test_highcolor_0025(int argc, char **argv);
int test_highcolor_0026(int argc, char **argv);
int test_highcolor_0027(int argc, char **argv);
int test_highcolor_0028(int argc, char **argv);
int test_highcolor_0029(int argc, char **argv);
int test_highcolor_0030(int argc, char **argv);
int test_highcolor_0031(int argc, char **argv);
int test_highcolor_0032(int argc, char **argv);
int test_highcolor_0033(int argc, char **argv);
int test_highcolor_0034(int argc, char **argv);
int test_highcolor_0035(int argc, char **argv);
int test_highcolor_0036(int argc, char **argv);
int test_highcolor_0037(int argc, char **argv);
int test_highcolor_0038(int argc, char **argv);
int test_highcolor_0039(int argc, char **argv);
int test_highcolor_0040(int argc, char **argv);
int test_highcolor_0041(int argc, char **argv);
int test_highcolor_0042(int argc, char **argv);
int test_highcolor_0043(int argc, char **argv);
int test_highcolor_0044(int argc, char **argv);
int test_highcolor_0045(int argc, char **argv);
int test_highcolor_0046(int argc, char **argv);
int test_highcolor_0047(int argc, char **argv);
int test_highcolor_0048(int argc, char **argv);
int test_highcolor_0049(int argc, char **argv);
int test_highcolor_0050(int argc, char **argv);
int test_highcolor_0051(int argc, char **argv);
int test_highcolor_0052(int argc, char **argv);
int test_highcolor_0053(int argc, char **argv);
int test_highcolor_0054(int argc, char **argv);
int test_highcolor_0055(int argc, char **argv);
int test_highcolor_0056(int argc, char **argv);
int test_highcolor_0057(int argc, char **argv);
int test_highcolor_0058(int argc, char **argv);
int test_highcolor_0059(int argc, char **argv);
int test_highcolor_0060(int argc, char **argv);
int test_highcolor_0061(int argc, char **argv);
int test_highcolor_0062(int argc, char **argv);
int test_highcolor_0063(int argc, char **argv);
int test_highcolor_0064(int argc, char **argv);
int test_highcolor_0065(int argc, char **argv);
int test_highcolor_0066(int argc, char **argv);
int test_highcolor_0067(int argc, char **argv);
int test_highcolor_0068(int argc, char **argv);
int test_highcolor_0069(int argc, char **argv);
int test_highcolor_0070(int argc, char **argv);
int test_highcolor_0071(int argc, char **argv);
int test_highcolor_0072(int argc, char **argv);
int test_highcolor_0073(int argc, char **argv);
int test_highcolor_0074(int argc, char **argv);
int test_highcolor_0075(int argc, char **argv);
int test_highcolor_0076(int argc, char **argv);
int test_highcolor_0077(int argc, char **argv);
int test_highcolor_0078(int argc, char **argv);
int test_highcolor_0079(int argc, char **argv);
int test_highcolor_0080(int argc, char **argv);
int test_highcolor_0081(int argc, char **argv);
int test_highcolor_0082(int argc, char **argv);
int test_highcolor_0083(int argc, char **argv);
int test_png_snapshot_0001(int argc, char **argv);
int test_png_snapshot_0002(int argc, char **argv);
int test_png_snapshot_0003(int argc, char **argv);
int test_png_snapshot_0004(int argc, char **argv);
int test_png_snapshot_0005(int argc, char **argv);
int test_png_snapshot_0006(int argc, char **argv);
int test_png_snapshot_0007(int argc, char **argv);
int test_png_snapshot_0008(int argc, char **argv);
int test_png_snapshot_0009(int argc, char **argv);
int test_hls_native_0001(int argc, char **argv);
int test_hls_native_0002(int argc, char **argv);
int test_hls_native_0003(int argc, char **argv);
int test_hls_native_0004(int argc, char **argv);
int test_hls_native_0005(int argc, char **argv);
int test_hls_native_0006(int argc, char **argv);
int test_hls_native_0007(int argc, char **argv);
int test_hls_native_0008(int argc, char **argv);
int test_hls_native_0009(int argc, char **argv);
int test_hls_native_0010(int argc, char **argv);
int test_hls_native_0011(int argc, char **argv);
int test_hls_native_0012(int argc, char **argv);
int test_hls_native_0013(int argc, char **argv);
int test_drcs_native_0001(int argc, char **argv);
int test_drcs_native_0002(int argc, char **argv);
int test_drcs_native_0003(int argc, char **argv);
int test_drcs_native_0004(int argc, char **argv);
int test_drcs_native_0005(int argc, char **argv);
int test_drcs_native_0006(int argc, char **argv);
int test_drcs_native_0007(int argc, char **argv);
int test_drcs_native_0008(int argc, char **argv);
int test_drcs_native_0009(int argc, char **argv);
int test_drcs_native_0010(int argc, char **argv);
int test_drcs_native_0011(int argc, char **argv);
int test_drcs_native_0012(int argc, char **argv);
int test_drcs_native_0013(int argc, char **argv);
int test_drcs_highcolor_0014(int argc, char **argv);
int test_drcs_highcolor_0015(int argc, char **argv);
int test_drcs_highcolor_0016(int argc, char **argv);
int test_drcs_highcolor_0017(int argc, char **argv);
int test_drcs_highcolor_0018(int argc, char **argv);
int test_drcs_highcolor_0019(int argc, char **argv);
int test_drcs_highcolor_0020(int argc, char **argv);
int test_drcs_highcolor_0021(int argc, char **argv);
int test_drcs_highcolor_0022(int argc, char **argv);
int test_drcs_highcolor_0023(int argc, char **argv);
int test_drcs_highcolor_0024(int argc, char **argv);
int test_map_cluster_0001(int argc, char **argv);
int test_map_cluster_0002(int argc, char **argv);
int test_map_cluster_0003(int argc, char **argv);
int test_map_cluster_0004(int argc, char **argv);
int test_map_cluster_0005(int argc, char **argv);
int test_map_cluster_0006(int argc, char **argv);
int test_map_cluster_0007(int argc, char **argv);
int test_map_cluster_0008(int argc, char **argv);
int test_map_cluster_0009(int argc, char **argv);
int test_map_cluster_0010(int argc, char **argv);
int test_map_cluster_0011(int argc, char **argv);
int test_map_cluster_0012(int argc, char **argv);
int test_map_cluster_0013(int argc, char **argv);
int test_cluster_precision_0001(int argc, char **argv);
int test_cluster_precision_0002(int argc, char **argv);
int test_cluster_precision_0003(int argc, char **argv);
int test_cluster_precision_0004(int argc, char **argv);
int test_cluster_precision_0005(int argc, char **argv);
int test_cluster_precision_0006(int argc, char **argv);
int test_cluster_precision_0007(int argc, char **argv);
int test_cluster_precision_0008(int argc, char **argv);
int test_cluster_precision_0009(int argc, char **argv);
int test_cluster_precision_0010(int argc, char **argv);
int test_cluster_precision_0011(int argc, char **argv);
int test_cluster_precision_0012(int argc, char **argv);
int test_cluster_precision_0013(int argc, char **argv);
int test_cluster_precision_0014(int argc, char **argv);

int test_clamp_0047(int argc, char **argv);
int test_clamp_0032(int argc, char **argv);
int test_filter_0015_filter_dither_6delta_lso2(int argc, char **argv);
int test_perturb_0002_api(int argc, char **argv);
int test_perturb_0001_weights(int argc, char **argv);
int test_perturb_0018_keep_lookup(int argc, char **argv);
int test_perturb_0017_keep_early(int argc, char **argv);
int test_perturb_0003_row_8bit(int argc, char **argv);
int test_perturb_0004_row_float(int argc, char **argv);
int test_filter_0016_filter_dither_6delta_jajuni(int argc, char **argv);
int test_filter_0017_filter_dither_6delta_stucki(int argc, char **argv);
int test_filter_0018_filter_dither_6delta_burkes(int argc, char **argv);
int test_filter_0019_filter_dither_6delta_sierra1(int argc, char **argv);
int test_filter_0020_filter_dither_6delta_sierra2(int argc, char **argv);
int test_filter_0021_filter_dither_6delta_sierra3(int argc, char **argv);
int test_filter_0022_filter_dither_6delta_a_dither(int argc, char **argv);
int test_filter_0023_filter_dither_6delta_x_dither(int argc, char **argv);
int test_filter_0024_filter_dither_6delta_bluenoise(int argc, char **argv);
int test_filter_0025_filter_dither_6delta_bluenoise_gpu(int argc, char **argv);
int test_filter_0026_filter_dither_6delta_diffusion_skip(int argc,
                                                        char **argv);
int test_filter_0027_filter_gpu_6delta_disabled_reject(int argc, char **argv);
int test_filter_0028_filter_dither_6delta_bluenoise_gpu_exact_plane(
    int argc,
    char **argv);
int test_filter_0029_dither_6delta_bluenoise_gpu_auto_fallback(
    int argc,
    char **argv);
int test_filter_0030_filter_palette_failure_output(int argc, char **argv);
int test_filter_0031_filter_binning(int argc, char **argv);
int test_filter_0032_filter_binning_failure_output(int argc, char **argv);
int test_filter_0033_filter_binning_exact(int argc, char **argv);
int test_filter_0034_filter_binning_sample_stream(int argc, char **argv);
int test_filter_0035_filter_binning_stream_format(int argc, char **argv);
int test_filter_0036_filter_binning_stream_transparency(int argc, char **argv);
int test_filter_0037_filter_binning_stream_float(int argc, char **argv);
int test_filter_0038_filter_binning_stream_empty(int argc, char **argv);
int test_filter_0039_filter_binning_stream_none(int argc, char **argv);
int test_filter_0040_filter_binning_stream_exact(int argc, char **argv);
int test_filter_0041_filter_binning_uniform_256(int argc, char **argv);
int test_filter_0042_filter_binning_stream_alpha_zero(int argc, char **argv);
int test_filter_0043_filter_dither_transparent_mask_fence_serial(
    int argc,
    char **argv);
int test_filter_0044_filter_dither_transparent_mask_fence_parallel(
    int argc,
    char **argv);
int test_filter_0045_filter_binning_stream_alpha_zero_disabled(
    int argc,
    char **argv);
int test_gpupal_0001_strtoul(int argc, char **argv);
int test_gpupal_0002_reserved_key(int argc, char **argv);
int test_gpupal_0003_fade_history(int argc, char **argv);
int test_gpupal_0004_auto_strength(int argc, char **argv);
int test_gpupal_0005_wire_rgb_none(int argc, char **argv);
int test_gpupal_0006_wire_rgb_bluenoise(int argc, char **argv);
int test_gpupal_0007_wire_rgb_eytzinger(int argc, char **argv);
int test_gpupal_0008_wire_hls_none(int argc, char **argv);
int test_gpupal_0009_wire_hls_bluenoise(int argc, char **argv);
int test_gpupal_0010_comparison_default(int argc, char **argv);
int test_gpupal_0011_comparison_short(int argc, char **argv);
int test_positional_0048_gpu_6delta(int argc, char **argv);
int test_bluenoise_0052_keep_offset(int argc, char **argv);
int test_gpudeq_0001_strtoul(int argc, char **argv);
int test_encoder_core_0001_encoder_core_factory(int argc, char **argv);
int test_or_enc_0027(int argc, char **argv);
int test_or_enc_0028(int argc, char **argv);
int test_or_enc_0029(int argc, char **argv);
int test_or_dec_0041(int argc, char **argv);
int test_pmap_0042(int argc, char **argv);
int test_pmap_0043(int argc, char **argv);
int test_pmap_0044(int argc, char **argv);
int test_pmap_0045(int argc, char **argv);
int test_pmap_0046(int argc, char **argv);
int test_pmap_0047(int argc, char **argv);
int test_pmap_0048(int argc, char **argv);
int test_pmap_0049(int argc, char **argv);
int test_pmap_0050(int argc, char **argv);
int test_pmap_0051(int argc, char **argv);
int test_pmap_0052(int argc, char **argv);
int test_or_enc_0007(int argc, char **argv);
int test_or_enc_0008(int argc, char **argv);
int test_or_enc_0009(int argc, char **argv);
int test_or_enc_0010(int argc, char **argv);
int test_or_enc_0011(int argc, char **argv);
int test_or_enc_0012(int argc, char **argv);
int test_or_enc_0013(int argc, char **argv);
int test_or_enc_0014(int argc, char **argv);
int test_or_enc_0015(int argc, char **argv);
int test_or_enc_0016(int argc, char **argv);
int test_or_enc_0017(int argc, char **argv);
int test_or_enc_0018(int argc, char **argv);
int test_or_enc_0019(int argc, char **argv);
int test_or_enc_0020(int argc, char **argv);
int test_or_enc_0021(int argc, char **argv);
int test_or_enc_0022(int argc, char **argv);
int test_or_enc_0023(int argc, char **argv);
int test_or_enc_0024(int argc, char **argv);
int test_or_enc_0025(int argc, char **argv);
int test_or_enc_0026(int argc, char **argv);
int test_or_dec_0022(int argc, char **argv);
int test_or_dec_0027(int argc, char **argv);
int test_or_dec_0028(int argc, char **argv);
int test_or_dec_0029(int argc, char **argv);
int test_or_dec_0030(int argc, char **argv);
int test_or_dec_0031(int argc, char **argv);
int test_or_dec_0032(int argc, char **argv);
int test_or_dec_0033(int argc, char **argv);
int test_or_dec_0034(int argc, char **argv);
int test_or_dec_0035(int argc, char **argv);
int test_or_dec_0036(int argc, char **argv);
int test_or_dec_0037(int argc, char **argv);
int test_or_dec_0038(int argc, char **argv);
int test_or_dec_0039(int argc, char **argv);
int test_or_dec_0040(int argc, char **argv);
int test_or_offset_cli(int argc, char **argv);
int test_encoder_core_0002_encoder_core_ormode_body_full_band(int argc,
                                                              char **argv);
int test_encoder_core_0003_encoder_core_ormode_body_tail_band(int argc,
                                                              char **argv);
int test_encoder_core_0004_encoder_core_ormode_body_bad_argument(int argc,
                                                                 char **argv);
int
test_encoder_core_0005_ormode_pipeline_body_match(int argc, char **argv);
int
test_encoder_core_0006_ormode_body_skip_empty_planes(int argc, char **argv);
int test_sixel_writer_0001_sixel_writer_factory(int argc, char **argv);
int test_frame_0001_frame_factory(int argc, char **argv);
int test_frame_0002_float_request(int argc, char **argv);
int test_frame_0006_extended(int argc, char **argv);
int test_frame_0014_clone_float(int argc, char **argv);
int test_frame_0015_atomic(int argc, char **argv);
int test_frame_0018_resampling(int argc, char **argv);
int test_frame_0054_geometry(int argc, char **argv);
int test_frame_0098_dynamic(int argc, char **argv);
int test_frame_0178_byte(int argc, char **argv);
int test_apng_0001_float(int argc, char **argv);
int test_frame_0239_alpha(int argc, char **argv);
int test_frame_0003_transparent_mask_clip(int argc, char **argv);
int test_frame_0004_transparent_mask_resize(int argc, char **argv);
int test_frame_0005_transparent_mask_resize_bilinear(int argc, char **argv);
int test_geometry_resampling_exact(int argc, char **argv);
int test_geometry_resampling_measurement_baseline(int argc, char **argv);
int test_plan_0001_resize_linear_float32(int argc, char **argv);
int test_plan_0002_resize_alloc_failure(int argc, char **argv);
int test_precision_0001(int argc, char **argv);
int test_precision_0002(int argc, char **argv);
int test_precision_0003(int argc, char **argv);
int test_precision_0004(int argc, char **argv);
int test_precision_0005(int argc, char **argv);
int test_precision_0006(int argc, char **argv);
int test_precision_0007(int argc, char **argv);
int test_precision_0008(int argc, char **argv);
int test_precision_0009(int argc, char **argv);
int test_precision_0018(int argc, char **argv);
int test_chunk_0001_chunk_factory(int argc, char **argv);
int test_chunk_cancel_callback(int argc, char **argv);
int test_timeline_0001_timeline_logger_factory(int argc, char **argv);
int test_timeline_0002_timeline_parallel_encode_decode(int argc, char **argv);
int test_timeline_0003_timeline_clock_origin(int argc, char **argv);
int test_timeline_0004_timeline_log_path_environment_probe(int argc,
                                                           char **argv);
int test_timeline_0005_timeline_line_policy_environment(int argc,
                                                        char **argv);
int test_platform_path_0001_path_to_libc_runtime(int argc, char **argv);
int test_threadpool_0001_threadpool_service(int argc, char **argv);
int test_threadpool_0002_thread_default_counts(int argc, char **argv);
int test_scale_0001_parallel_factor_environment(int argc, char **argv);
int test_scale_0002_parallel_min_bytes_environment(int argc, char **argv);
int test_scale_0003_resample_alloc_failure(int argc, char **argv);
int test_scale_0004_g8_rgb888(int argc, char **argv);
int test_scale_0005_rgb565_rgb888(int argc, char **argv);
int test_scale_0006_rgba_rgb888(int argc, char **argv);
int test_decoder_0001_decoder_parallel_split_after_newline(int argc,
                                                           char **argv);
int test_decoder_0002_decoder_ormode_raw_overlay(int argc, char **argv);
int test_decoder_0003_decoder_ormode_wide_overlay(int argc, char **argv);
int test_decoder_0004_decoder_ormode_direct_overlay(int argc, char **argv);
int test_decoder_0005_decoder_ormode_repeat_overlay(int argc, char **argv);
int test_decoder_0006_decoder_ormode_legacy_api(int argc, char **argv);
int test_decoder_0007_decoder_ormode_parallel_request(int argc, char **argv);
int test_decoder_0008_decoder_pixels_output_formats(int argc, char **argv);
int test_decoder_0009_decoder_pixels_trust_raster_clip(int argc, char **argv);
int test_decoder_0010_decoder_kundither_default(int argc, char **argv);
int test_decoder_0011_decoder_kundither_similarity(int argc, char **argv);
int test_decoder_0012_decoder_kundither_edge(int argc, char **argv);
int test_decoder_0013_decoder_kundither_parallel_matches_scalar(int argc,
                                                                char **argv);
int test_decoder_0014_kundither_fast4_matches_scalar(int argc, char **argv);
int test_decoder_0015_decoder_kundither_fast4_fused_matches_scalar(int argc,
                                                                   char **argv);
int test_decoder_0016_decoder_parallel_fallback_keeps_image_clean(int argc,
                                                                  char **argv);
int test_decoder_0017_decoder_parallel_sparse_split_matches_serial(int argc,
                                                                   char **argv);
int
test_decoder_0018_direct_sparse_unpainted(
    int argc,
    char **argv);
int
test_decoder_0019_direct_repeat_overflow(
    int argc,
    char **argv);
int test_decoder_0020_decoder_pixels_body_api(int argc, char **argv);
int test_decoder_0021_decoder_selective_blur_threshold(int argc, char **argv);
int test_decoder_0023_decoder_high_color_dequantize_bypass(int argc, char **argv);
int
test_decoder_0024_decoder_parallel_skew_environment(int argc, char **argv);
int test_decoder_0025_decoder_paint_create_failure_clean(int argc,
                                                         char **argv);
int test_decoder_0026_decoder_paint_overlap_fallback_clean(int argc,
                                                           char **argv);
int test_dither_0001_dither_policy(int argc, char **argv);
int test_lookup_0001_lookup_policy(int argc, char **argv);
int test_lookup_0012_packing_registry_direct(int argc, char **argv);
int test_lookup_0013_parallel_dense_cache(int argc, char **argv);

int test_probe_0001_probe_parse(int argc, char **argv);
int test_status_0001_status_timeout(int argc, char **argv);
int test_icc_0001_icc_builtin_rgb_gray_v4_paths(int argc, char **argv);
int test_icc_0002_icc_builtin_mab_mba_a2b0_paths(int argc, char **argv);
int test_icc_0003_icc_builtin_a2b_intent_paths(int argc, char **argv);
int test_icc_0004_icc_builtin_b2a_slot_paths(int argc, char **argv);
int test_icc_0005_icc_builtin_device_to_device_intent_paths(int argc,
                                                             char **argv);
int test_icc_0006_icc_builtin_pixel_format_matrix(int argc, char **argv);
int test_icc_0007_icc_builtin_float_special_values(int argc, char **argv);
int test_icc_0008_icc_builtin_d2b_b2d_legacy_lut(int argc, char **argv);

int test_palette_0001_kmeans_init(int argc, char **argv);
int test_palette_0002_kmedoids_constraints(int argc, char **argv);
int test_palette_0003_kcenter_constraints(int argc, char **argv);
int test_palette_0004_palette_factory(int argc, char **argv);
int test_palette_0005_cover_anchor(int argc, char **argv);
int test_palfb_0006_conversion(int argc, char **argv);
int test_palfb_0007_output(int argc, char **argv);
int test_palette_0008_palette_policy_resolution(int argc, char **argv);
int test_palette_0025_sample_stream(int argc, char **argv);
int test_palette_0026_sampling_auto(int argc, char **argv);
int test_palette_0028_sampling_schedule(int argc, char **argv);
int test_palette_0029_sampling_fallback(int argc, char **argv);
int test_palette_0030_binning_policy(int argc, char **argv);
int test_palette_0031_weighted_point_set(int argc, char **argv);
int test_palette_0032_kmeans_binning_policy_output(int argc, char **argv);
int test_palette_0033_quantizer_capabilities(int argc, char **argv);
int test_palette_0034_palette_stage_resolvers(int argc, char **argv);
int test_palette_0037_palette_sampling_indexed_frame(int argc, char **argv);
int test_palette_0040_palette_build_attempt_isolation(int argc, char **argv);
int test_palette_0041_kcenter_binning_filter(int argc, char **argv);
int test_palette_0042_kcenter_large_buffer(int argc, char **argv);
int test_palette_0043_kcenter_float_grid_values(int argc, char **argv);
int test_palette_0044_kcenter_packed_alpha_layouts(int argc, char **argv);
int test_palette_0045_snap_grid(int argc, char **argv);
int test_palette_0048_snap_partial(int argc, char **argv);

int test_cli_0029_cli_token_is_known_option(int argc, char **argv);
int test_cli_0030_cli_option_requires_argument(int argc, char **argv);
int test_cli_0031_cli_guard_missing_argument(int argc, char **argv);
int test_cli_0032_cli_gpu_policy_option(int argc, char **argv);
int test_cli_0033_consumer_scope(int argc, char **argv);
int test_cli_0034_cli_terminal_policy_option(int argc, char **argv);
int test_cli_0035_lookup_initials(int argc, char **argv);
int test_cli_0036_color_wire(int argc, char **argv);
int test_cli_0037_color_parser(int argc, char **argv);
int test_cli_0038_color_tty(int argc, char **argv);
int test_cli_0039_color_auto(int argc, char **argv);
int test_cli_0040_color_job(int argc, char **argv);
int test_cli_stdin_interrupt(int argc, char **argv);
int test_rgb101_spatial(int argc, char **argv);
int test_palette_space(int argc, char **argv);
int test_palette_sampling_float(int argc, char **argv);
int test_palette_binning_float(int argc, char **argv);
int test_palette_merge_float(int argc, char **argv);
int test_palette_cover_native(int argc, char **argv);
int test_cover_mask(int argc, char **argv);
int test_native_snap(int argc, char **argv);
int test_icc_output(int argc, char **argv);
int test_gamut_0020_edge_srgb_reference(int argc, char **argv);
int test_gamut_0021_edge_p3_reference(int argc, char **argv);
int test_gamut_0022_edge_adobe_reference(int argc, char **argv);
int test_gamut_0023_edge_dci_reference(int argc, char **argv);
int test_gamut_0024_edge_identity(int argc, char **argv);
int test_gamut_0025_edge_endpoints(int argc, char **argv);
int test_gamut_0026_edge_nonfinite_atomic(int argc, char **argv);
int test_gamut_0027_edge_matrix_icc(int argc, char **argv);
int test_gamut_0028_edge_invalid_boundary(int argc, char **argv);
int test_gamut_0029_edge_bounds(int argc, char **argv);
int test_gamut_0030_edge_full_frame_samples(int argc, char **argv);
int test_gamut_0031_edge_adaptive_samples(int argc, char **argv);
int test_gamut_0032_edge_oklab_workspace(int argc, char **argv);
int test_gamut_0033_edge_highcolor(int argc, char **argv);
int test_gamut_0034_edge_lut_rejected(int argc, char **argv);
int test_gamut_0035_edge_environment_precedence(int argc, char **argv);
int test_gamut_0036_edge_encoder_isolation(int argc, char **argv);
int test_gamut_0038_edge_linear_reference(int argc, char **argv);
int test_gamut_0039_ray_srgb_reference(int argc, char **argv);
int test_gamut_0040_ray_p3_reference(int argc, char **argv);
int test_gamut_0041_ray_adobe_reference(int argc, char **argv);
int test_gamut_0042_ray_dci_reference(int argc, char **argv);
int test_gamut_0043_ray_identity(int argc, char **argv);
int test_gamut_0044_ray_endpoints(int argc, char **argv);
int test_gamut_0045_ray_nonfinite_atomic(int argc, char **argv);
int test_gamut_0046_ray_matrix_icc(int argc, char **argv);
int test_gamut_0047_ray_bounds(int argc, char **argv);
int test_gamut_0048_ray_full_frame_samples(int argc, char **argv);
int test_gamut_0049_ray_adaptive_samples(int argc, char **argv);
int test_gamut_0050_ray_oklab_workspace(int argc, char **argv);
int test_gamut_0051_ray_highcolor(int argc, char **argv);
int test_gamut_0052_ray_lut_rejected(int argc, char **argv);
int test_gamut_0053_ray_environment_precedence(int argc, char **argv);
int test_gamut_0054_ray_encoder_isolation(int argc, char **argv);
int test_gamut_0056_ray_geometry(int argc, char **argv);
int test_gamut_0057_ray_cache_reset(int argc, char **argv);
int test_gamut_0058_ray_linear_reference(int argc, char **argv);
int test_gamut_0059_ray_convergence(int argc, char **argv);
int test_gamut_0013_matrix_icc(int argc, char **argv);
int test_gamut_0014_lut_rejected(int argc, char **argv);
int test_gamut_0015_encoder_isolation(int argc, char **argv);
int test_gamut_0016_environment_precedence(int argc, char **argv);
int test_gamut_0017_palette_continuous(int argc, char **argv);
int test_gamut_0018_policy_cache_reset(int argc, char **argv);

int test_gamut_0009_full_frame_samples(int argc, char **argv);
int test_gamut_0010_adaptive_samples(int argc, char **argv);
int test_gamut_0011_oklab_workspace(int argc, char **argv);
int test_gamut_0012_highcolor(int argc, char **argv);

int test_gamut_0001_srgb_identity(int argc, char **argv);
int test_gamut_0002_p3_extended_identity(int argc, char **argv);
int test_gamut_0003_srgb_reference(int argc, char **argv);
int test_gamut_0004_p3_reference(int argc, char **argv);
int test_gamut_0005_adobe_reference(int argc, char **argv);
int test_gamut_0006_dci_reference(int argc, char **argv);
int test_gamut_0007_nonfinite_atomic(int argc, char **argv);
int test_gamut_0008_policy_atomicity(int argc, char **argv);

int test_destination_snap(int argc, char **argv);
int test_p3_snap_grid(int argc, char **argv);
int test_p3_snap_pipeline(int argc, char **argv);
int test_ward_scan_populations(int argc, char **argv);
int test_extended_lookup(int argc, char **argv);
int test_positional_0059(int argc, char **argv);
int test_positional_0060(int argc, char **argv);
int test_positional_0061(int argc, char **argv);
int test_positional_0076(int argc, char **argv);
int test_positional_0075(int argc, char **argv);
int test_positional_0074(int argc, char **argv);
int test_positional_0073(int argc, char **argv);
int test_positional_0072(int argc, char **argv);
int test_positional_0071(int argc, char **argv);
int test_positional_0070(int argc, char **argv);
int test_positional_0069(int argc, char **argv);
int test_positional_0068(int argc, char **argv);
int test_positional_0067(int argc, char **argv);
int test_positional_0066(int argc, char **argv);
int test_positional_0065(int argc, char **argv);
int test_positional_0001(int argc, char **argv);
int test_positional_0002(int argc, char **argv);
int test_positional_0003(int argc, char **argv);
int test_positional_0004(int argc, char **argv);
int test_positional_0005(int argc, char **argv);
int test_positional_0006(int argc, char **argv);
int test_positional_0007(int argc, char **argv);
int test_positional_0008(int argc, char **argv);
int test_positional_0009(int argc, char **argv);
int test_positional_0010(int argc, char **argv);
int test_positional_0011(int argc, char **argv);
int test_positional_0012(int argc, char **argv);
int test_positional_0013(int argc, char **argv);
int test_positional_0014(int argc, char **argv);
int test_positional_0015(int argc, char **argv);
int test_positional_0016(int argc, char **argv);
int test_positional_0017(int argc, char **argv);
int test_positional_0018(int argc, char **argv);
int test_positional_0019(int argc, char **argv);
int test_positional_0020(int argc, char **argv);
int test_positional_0021(int argc, char **argv);
int test_positional_0022(int argc, char **argv);
int test_positional_0023(int argc, char **argv);
int test_positional_0024(int argc, char **argv);
int test_positional_0025(int argc, char **argv);
int test_positional_0026(int argc, char **argv);
int test_positional_0027(int argc, char **argv);
int test_positional_0028(int argc, char **argv);
int test_positional_0029(int argc, char **argv);
int test_positional_0030(int argc, char **argv);
int test_positional_0031(int argc, char **argv);
int test_positional_0032(int argc, char **argv);
int test_positional_0033(int argc, char **argv);
int test_positional_0034(int argc, char **argv);
int test_positional_0035(int argc, char **argv);
int test_positional_0036(int argc, char **argv);
int test_positional_0037(int argc, char **argv);
int test_positional_0038(int argc, char **argv);
int test_positional_0039(int argc, char **argv);
int test_positional_0040(int argc, char **argv);
int test_positional_0041(int argc, char **argv);
int test_positional_0042(int argc, char **argv);
int test_positional_0043(int argc, char **argv);
int test_positional_0044(int argc, char **argv);
int test_positional_0045(int argc, char **argv);
int test_positional_0046(int argc, char **argv);
int test_positional_0047(int argc, char **argv);
int test_positional_0048(int argc, char **argv);
int test_positional_0049(int argc, char **argv);
int test_positional_0050(int argc, char **argv);
int test_positional_0051(int argc, char **argv);
int test_positional_0052(int argc, char **argv);
int test_positional_0053(int argc, char **argv);
int test_positional_0054(int argc, char **argv);
int test_positional_0055(int argc, char **argv);
int test_positional_0056(int argc, char **argv);
int test_positional_0057(int argc, char **argv);
int test_positional_0058(int argc, char **argv);
int test_map_precision_0001(int argc, char **argv);
int test_map_precision_0002(int argc, char **argv);
int test_map_precision_0003(int argc, char **argv);
int test_map_precision_0004(int argc, char **argv);
int test_map_precision_0005(int argc, char **argv);
int test_map_precision_0006(int argc, char **argv);
int test_map_precision_0007(int argc, char **argv);
int test_map_precision_0008(int argc, char **argv);
int test_map_precision_0009(int argc, char **argv);
int test_map_precision_0010(int argc, char **argv);
int test_map_precision_0011(int argc, char **argv);
int test_map_precision_0012(int argc, char **argv);
int test_map_precision_0013(int argc, char **argv);
int test_map_precision_0014(int argc, char **argv);
int test_map_precision_0015(int argc, char **argv);
int test_map_precision_0016(int argc, char **argv);
int test_map_precision_0017(int argc, char **argv);
int test_map_precision_0018(int argc, char **argv);
int test_map_precision_0019(int argc, char **argv);
int test_map_precision_0020(int argc, char **argv);
int test_map_precision_0021(int argc, char **argv);
int test_map_precision_0022(int argc, char **argv);
int test_map_precision_0023(int argc, char **argv);
int test_map_precision_0024(int argc, char **argv);
int test_map_solver_0001(int argc, char **argv);
int test_map_solver_0002(int argc, char **argv);
int test_map_solver_0003(int argc, char **argv);
int test_map_solver_0004(int argc, char **argv);
int test_map_solver_0005(int argc, char **argv);
int test_map_solver_0006(int argc, char **argv);
int test_map_solver_0007(int argc, char **argv);
int test_map_solver_0008(int argc, char **argv);
int test_map_solver_0009(int argc, char **argv);
int test_map_solver_0010(int argc, char **argv);
int test_map_solver_0011(int argc, char **argv);
int test_map_solver_0012(int argc, char **argv);
int test_map_solver_0013(int argc, char **argv);
int test_map_solver_0014(int argc, char **argv);
int test_map_solver_0015(int argc, char **argv);
int test_map_solver_0016(int argc, char **argv);
int test_map_solver_0017(int argc, char **argv);
int test_map_solver_0018(int argc, char **argv);
int test_map_solver_0019(int argc, char **argv);
int test_map_solver_0020(int argc, char **argv);
int test_map_solver_0021(int argc, char **argv);
int test_map_solver_0022(int argc, char **argv);
int test_map_solver_0023(int argc, char **argv);
int test_map_solver_0024(int argc, char **argv);
int test_map_solver_0025(int argc, char **argv);
int test_map_solver_0026(int argc, char **argv);
int test_map_solver_0027(int argc, char **argv);
int test_map_solver_0028(int argc, char **argv);
int test_map_solver_0029(int argc, char **argv);
int test_map_solver_0030(int argc, char **argv);
int test_map_solver_0031(int argc, char **argv);
int test_map_solver_0032(int argc, char **argv);
int test_map_solver_0033(int argc, char **argv);
int test_map_solver_0034(int argc, char **argv);
int test_map_solver_0035(int argc, char **argv);
int test_map_solver_0036(int argc, char **argv);
int test_map_native_0001(int argc, char **argv);
int test_map_native_0002(int argc, char **argv);
int test_map_native_0003(int argc, char **argv);
int test_map_native_0004(int argc, char **argv);
int test_map_native_0005(int argc, char **argv);
int test_map_native_0006(int argc, char **argv);
int test_map_native_0007(int argc, char **argv);
int test_map_native_0008(int argc, char **argv);
int test_map_native_0009(int argc, char **argv);
int test_map_native_0010(int argc, char **argv);
int test_map_native_0011(int argc, char **argv);
int test_map_native_0012(int argc, char **argv);
int test_map_native_0013(int argc, char **argv);
int test_map_native_0014(int argc, char **argv);
int test_map_native_0015(int argc, char **argv);
int test_map_native_0016(int argc, char **argv);
int test_map_native_0017(int argc, char **argv);
int test_map_native_0018(int argc, char **argv);
int test_map_native_0019(int argc, char **argv);
int test_map_native_0020(int argc, char **argv);
int test_map_native_0021(int argc, char **argv);
int test_map_native_0022(int argc, char **argv);
int test_map_native_0023(int argc, char **argv);
int test_map_native_0024(int argc, char **argv);
int test_map_native_0025(int argc, char **argv);
int test_auto_native_0001(int argc, char **argv);
int test_auto_native_0002(int argc, char **argv);
int test_auto_native_0003(int argc, char **argv);
int test_auto_native_0004(int argc, char **argv);
int test_auto_native_0005(int argc, char **argv);
int test_auto_native_0006(int argc, char **argv);
int test_auto_native_0007(int argc, char **argv);
int test_auto_native_0008(int argc, char **argv);
int test_auto_native_0009(int argc, char **argv);
int test_auto_native_0010(int argc, char **argv);
int test_auto_native_0011(int argc, char **argv);
int test_auto_native_0012(int argc, char **argv);
int test_auto_native_0013(int argc, char **argv);
int test_auto_native_0014(int argc, char **argv);
int test_auto_native_0015(int argc, char **argv);
int test_auto_native_0016(int argc, char **argv);
int test_auto_native_0017(int argc, char **argv);
int test_auto_native_0018(int argc, char **argv);
int test_auto_native_0019(int argc, char **argv);
int test_auto_native_0020(int argc, char **argv);
int test_auto_native_0021(int argc, char **argv);
int test_auto_native_0022(int argc, char **argv);
int test_auto_native_0023(int argc, char **argv);
int test_heckbert_native_0033(int argc, char **argv);
int test_heckbert_native_0034(int argc, char **argv);
int test_heckbert_native_0032(int argc, char **argv);
int test_heckbert_native_0030(int argc, char **argv);
int test_heckbert_native_0031(int argc, char **argv);
int test_heckbert_native_0001(int argc, char **argv);
int test_heckbert_native_0002(int argc, char **argv);
int test_heckbert_native_0003(int argc, char **argv);
int test_heckbert_native_0004(int argc, char **argv);
int test_heckbert_native_0005(int argc, char **argv);
int test_heckbert_native_0006(int argc, char **argv);
int test_heckbert_native_0007(int argc, char **argv);
int test_heckbert_native_0008(int argc, char **argv);
int test_heckbert_native_0009(int argc, char **argv);
int test_heckbert_native_0010(int argc, char **argv);
int test_heckbert_native_0011(int argc, char **argv);
int test_heckbert_native_0012(int argc, char **argv);
int test_heckbert_native_0013(int argc, char **argv);
int test_heckbert_native_0014(int argc, char **argv);
int test_heckbert_native_0015(int argc, char **argv);
int test_heckbert_native_0016(int argc, char **argv);
int test_heckbert_native_0017(int argc, char **argv);
int test_heckbert_native_0018(int argc, char **argv);
int test_heckbert_native_0019(int argc, char **argv);
int test_heckbert_native_0020(int argc, char **argv);
int test_heckbert_native_0021(int argc, char **argv);
int test_heckbert_native_0022(int argc, char **argv);
int test_heckbert_native_0023(int argc, char **argv);
int test_heckbert_native_0024(int argc, char **argv);
int test_heckbert_native_0025(int argc, char **argv);
int test_heckbert_native_0026(int argc, char **argv);
int test_heckbert_native_0027(int argc, char **argv);
int test_heckbert_native_0028(int argc, char **argv);
int test_heckbert_native_0029(int argc, char **argv);
int test_fh_lookup_0020(int argc, char **argv);
int test_fh_lookup_0021(int argc, char **argv);
int test_fh_lookup_0022(int argc, char **argv);
int test_fh_lookup_0023(int argc, char **argv);
int test_mono_lookup_0001(int argc, char **argv);
int test_mono_lookup_0002(int argc, char **argv);
int test_mono_lookup_0003(int argc, char **argv);
int test_mono_lookup_0004(int argc, char **argv);
int test_mono_lookup_0005(int argc, char **argv);
int test_mono_lookup_0006(int argc, char **argv);
int test_mono_lookup_0007(int argc, char **argv);
int test_mono_lookup_0008(int argc, char **argv);
int test_mono_lookup_0009(int argc, char **argv);
int test_mono_lookup_0010(int argc, char **argv);
int test_fixed_mask_0001(int argc, char **argv);
int test_fixed_mask_0002(int argc, char **argv);
int test_fixed_mask_0003(int argc, char **argv);
int test_fixed_mask_0004(int argc, char **argv);
int test_fixed_mask_0005(int argc, char **argv);
int test_fixed_mask_0006(int argc, char **argv);
int test_fixed_mask_0007(int argc, char **argv);
int test_fixed_mask_0008(int argc, char **argv);
int test_fixed_mask_0009(int argc, char **argv);
int test_fixed_mask_0010(int argc, char **argv);
int test_fixed_mask_0011(int argc, char **argv);
int test_fixed_mask_0012(int argc, char **argv);
int test_fixed_mask_0013(int argc, char **argv);
int test_fixed_mask_0014(int argc, char **argv);
int test_fixed_mask_0015(int argc, char **argv);
int test_fixed_mask_0016(int argc, char **argv);
int test_fixed_mask_0017(int argc, char **argv);
int test_fixed_mask_0018(int argc, char **argv);
int test_fixed_mask_0019(int argc, char **argv);
int test_fixed_mask_0020(int argc, char **argv);
int test_fixed_mask_0021(int argc, char **argv);
int test_fixed_mask_0022(int argc, char **argv);
int test_fixed_mask_0023(int argc, char **argv);
int test_fixed_mask_0024(int argc, char **argv);
int test_fixed_mask_0025(int argc, char **argv);
int test_fixed_mask_0026(int argc, char **argv);
int test_fixed_mask_0027(int argc, char **argv);
int test_fixed_mask_0028(int argc, char **argv);
int test_fixed_mask_0029(int argc, char **argv);
int test_fixed_mask_0030(int argc, char **argv);
int test_anim_native_0001(int argc, char **argv);
int test_anim_native_0002(int argc, char **argv);
int test_anim_native_0003(int argc, char **argv);
int test_anim_native_0004(int argc, char **argv);
int test_anim_native_0005(int argc, char **argv);
int test_anim_native_0006(int argc, char **argv);
int test_anim_native_0007(int argc, char **argv);
int test_anim_native_0008(int argc, char **argv);
int test_anim_native_0009(int argc, char **argv);
int test_anim_native_0010(int argc, char **argv);
int test_anim_native_0011(int argc, char **argv);
int test_anim_native_0012(int argc, char **argv);
int test_anim_native_0013(int argc, char **argv);
int test_anim_native_0014(int argc, char **argv);
int test_anim_native_0015(int argc, char **argv);
int test_anim_native_0016(int argc, char **argv);
int test_anim_native_0017(int argc, char **argv);
int test_fixed_animation_0001(int argc, char **argv);
int test_fixed_animation_0002(int argc, char **argv);
int test_fixed_animation_0003(int argc, char **argv);
int test_fixed_animation_0004(int argc, char **argv);
int test_fixed_animation_0005(int argc, char **argv);
int test_fixed_animation_0006(int argc, char **argv);
int test_fixed_animation_0007(int argc, char **argv);
int test_fixed_animation_0008(int argc, char **argv);
int test_fixed_animation_0009(int argc, char **argv);
int test_fixed_animation_0010(int argc, char **argv);
int test_fixed_animation_0011(int argc, char **argv);
int test_fixed_animation_0012(int argc, char **argv);
int test_fixed_animation_0013(int argc, char **argv);
int test_fixed_animation_0014(int argc, char **argv);
int test_fixed_animation_0015(int argc, char **argv);
int test_fixed_animation_0016(int argc, char **argv);
int test_fixed_animation_0017(int argc, char **argv);
int test_fixed_animation_0018(int argc, char **argv);
int test_fixed_animation_0019(int argc, char **argv);
int test_fixed_animation_0020(int argc, char **argv);
int test_fixed_animation_0021(int argc, char **argv);
int test_fixed_animation_0022(int argc, char **argv);
int test_fixed_animation_0023(int argc, char **argv);
int test_fixed_animation_0024(int argc, char **argv);
int test_fixed_animation_0025(int argc, char **argv);
int test_fixed_animation_0026(int argc, char **argv);
int test_auto_cover_0001(int argc, char **argv);
int test_auto_cover_0002(int argc, char **argv);
int test_auto_cover_0003(int argc, char **argv);
int test_auto_cover_0004(int argc, char **argv);
int test_auto_cover_0005(int argc, char **argv);
int test_auto_cover_0006(int argc, char **argv);
int test_auto_cover_0007(int argc, char **argv);
int test_auto_cover_0008(int argc, char **argv);
int test_auto_cover_0009(int argc, char **argv);
int test_auto_cover_0010(int argc, char **argv);
int test_auto_cover_0011(int argc, char **argv);
int test_auto_cover_0012(int argc, char **argv);
int test_auto_cover_0013(int argc, char **argv);
int test_auto_cover_0014(int argc, char **argv);
int test_auto_cover_0015(int argc, char **argv);
int test_auto_cover_0016(int argc, char **argv);
int test_auto_cover_0017(int argc, char **argv);
int test_auto_cover_0018(int argc, char **argv);
int test_auto_cover_0019(int argc, char **argv);
int test_auto_cover_0020(int argc, char **argv);
int test_auto_cover_0021(int argc, char **argv);
int test_auto_cover_0022(int argc, char **argv);
int test_auto_cover_0023(int argc, char **argv);
int test_auto_cover_0024(int argc, char **argv);
int test_auto_cover_0025(int argc, char **argv);
int test_anim_native_0018(int argc, char **argv);
int test_anim_native_0019(int argc, char **argv);
int test_anim_native_0020(int argc, char **argv);
int test_anim_native_0021(int argc, char **argv);
int test_packed_0001(int argc, char **argv);
int test_packed_0002(int argc, char **argv);
int test_packed_0003(int argc, char **argv);
int test_packed_0004(int argc, char **argv);
int test_packed_0005(int argc, char **argv);
int test_packed_0006(int argc, char **argv);
int test_packed_0007(int argc, char **argv);
int test_packed_0008(int argc, char **argv);
int test_packed_0009(int argc, char **argv);
int test_packed_0010(int argc, char **argv);
int test_packed_0011(int argc, char **argv);
int test_packed_0012(int argc, char **argv);
int test_packed_0013(int argc, char **argv);
int test_packed_0014(int argc, char **argv);
int test_packed_0015(int argc, char **argv);
int test_packed_0016(int argc, char **argv);
int test_packed_0017(int argc, char **argv);
int test_packed_0018(int argc, char **argv);
int test_packed_0019(int argc, char **argv);
int test_packed_0020(int argc, char **argv);
int test_packed_0021(int argc, char **argv);
int test_packed_0022(int argc, char **argv);
int test_packed_0023(int argc, char **argv);
int test_packed_0024(int argc, char **argv);
int test_packed_0025(int argc, char **argv);
int test_packed_0026(int argc, char **argv);
int test_packed_0027(int argc, char **argv);
int test_packed_0028(int argc, char **argv);
int test_packed_0029(int argc, char **argv);
int test_packed_0030(int argc, char **argv);
int test_packed_0031(int argc, char **argv);
int test_packed_0032(int argc, char **argv);
int test_packed_0033(int argc, char **argv);
int test_packed_0034(int argc, char **argv);
int test_packed_0035(int argc, char **argv);
int test_alpha_native_0001(int argc, char **argv);
int test_alpha_native_0002(int argc, char **argv);
int test_alpha_native_0003(int argc, char **argv);
int test_alpha_native_0004(int argc, char **argv);
int test_alpha_native_0005(int argc, char **argv);
int test_alpha_native_0006(int argc, char **argv);
int test_alpha_native_0007(int argc, char **argv);
int test_alpha_native_0008(int argc, char **argv);
int test_alpha_native_0009(int argc, char **argv);
int test_alpha_native_0010(int argc, char **argv);
int test_alpha_native_0011(int argc, char **argv);
int test_alpha_native_0012(int argc, char **argv);
int test_alpha_native_0013(int argc, char **argv);
int test_alpha_native_0014(int argc, char **argv);
int test_alpha_native_0015(int argc, char **argv);
int test_alpha_native_0016(int argc, char **argv);
int test_alpha_native_0017(int argc, char **argv);
int test_alpha_native_0018(int argc, char **argv);
int test_alpha_native_0019(int argc, char **argv);
int test_alpha_native_0020(int argc, char **argv);
int test_alpha_native_0021(int argc, char **argv);
int test_alpha_native_0022(int argc, char **argv);
int test_alpha_native_0023(int argc, char **argv);
int test_alpha_native_0024(int argc, char **argv);
int test_alpha_native_0025(int argc, char **argv);
int test_alpha_native_0026(int argc, char **argv);
int test_alpha_native_0027(int argc, char **argv);
int test_alpha_native_0028(int argc, char **argv);
int test_alpha_native_0029(int argc, char **argv);
int test_alpha_native_0030(int argc, char **argv);
int test_alpha_native_0031(int argc, char **argv);
int test_alpha_native_0032(int argc, char **argv);
int test_alpha_native_0033(int argc, char **argv);
int test_alpha_native_0034(int argc, char **argv);
int test_mono_native_0001(int argc, char **argv);
int test_mono_native_0002(int argc, char **argv);
int test_mono_native_0003(int argc, char **argv);
int test_mono_native_0004(int argc, char **argv);
int test_mono_native_0005(int argc, char **argv);
int test_mono_native_0006(int argc, char **argv);
int test_mono_native_0007(int argc, char **argv);
int test_mono_native_0008(int argc, char **argv);
int test_mono_native_0009(int argc, char **argv);
int test_mono_native_0010(int argc, char **argv);
int test_mono_native_0011(int argc, char **argv);
int test_mono_native_0012(int argc, char **argv);
int test_mono_native_0013(int argc, char **argv);
int test_mono_native_0014(int argc, char **argv);
int test_mono_native_0015(int argc, char **argv);
int test_mono_native_0016(int argc, char **argv);
int test_mono_native_0017(int argc, char **argv);
int test_mono_native_0018(int argc, char **argv);
int test_mono_native_0019(int argc, char **argv);
int test_mono_native_0020(int argc, char **argv);
int test_mono_native_0021(int argc, char **argv);
int test_mono_native_0022(int argc, char **argv);
int test_mono_native_0023(int argc, char **argv);
int test_mono_native_0024(int argc, char **argv);
int test_mono_native_0025(int argc, char **argv);
int test_mono_native_0026(int argc, char **argv);
int test_mono_native_0027(int argc, char **argv);
int test_mono_native_0028(int argc, char **argv);
int test_mono_native_0029(int argc, char **argv);
int test_mono_native_0030(int argc, char **argv);
int test_mono_native_0031(int argc, char **argv);
int test_builtin_native_0001(int argc, char **argv);
int test_builtin_native_0002(int argc, char **argv);
int test_builtin_native_0003(int argc, char **argv);
int test_builtin_native_0004(int argc, char **argv);
int test_builtin_native_0005(int argc, char **argv);
int test_builtin_native_0006(int argc, char **argv);
int test_builtin_native_0007(int argc, char **argv);
int test_builtin_native_0008(int argc, char **argv);
int test_builtin_native_0009(int argc, char **argv);
int test_builtin_native_0010(int argc, char **argv);
int test_builtin_native_0011(int argc, char **argv);
int test_builtin_native_0012(int argc, char **argv);
int test_builtin_native_0013(int argc, char **argv);
int test_builtin_native_0014(int argc, char **argv);
int test_builtin_native_0015(int argc, char **argv);
int test_builtin_native_0016(int argc, char **argv);
int test_builtin_native_0017(int argc, char **argv);
int test_builtin_native_0018(int argc, char **argv);
int test_builtin_native_0019(int argc, char **argv);
int test_builtin_native_0020(int argc, char **argv);
int test_builtin_native_0021(int argc, char **argv);
int test_builtin_native_0022(int argc, char **argv);
int test_builtin_native_0023(int argc, char **argv);
int test_builtin_native_0024(int argc, char **argv);
int test_builtin_native_0025(int argc, char **argv);
int test_builtin_native_0026(int argc, char **argv);
int test_output_native_0001(int argc, char **argv);
int test_output_native_0002(int argc, char **argv);
int test_output_native_0003(int argc, char **argv);
int test_output_native_0004(int argc, char **argv);
int test_output_native_0005(int argc, char **argv);
int test_output_native_0006(int argc, char **argv);
int test_output_native_0007(int argc, char **argv);
int test_output_native_0008(int argc, char **argv);
int test_output_native_0009(int argc, char **argv);
int test_output_native_0010(int argc, char **argv);
int test_output_native_0011(int argc, char **argv);
int test_output_native_0012(int argc, char **argv);
int test_output_native_0013(int argc, char **argv);
int test_output_native_0014(int argc, char **argv);
int test_output_native_0015(int argc, char **argv);
int test_output_native_0016(int argc, char **argv);
int test_output_native_0017(int argc, char **argv);
int test_output_native_0018(int argc, char **argv);
int test_output_native_0019(int argc, char **argv);
int test_output_native_0020(int argc, char **argv);
int test_output_native_0021(int argc, char **argv);
int test_output_native_0022(int argc, char **argv);
int test_output_native_0023(int argc, char **argv);
int test_output_native_0024(int argc, char **argv);
int test_output_native_0025(int argc, char **argv);
int test_output_native_0026(int argc, char **argv);
int test_output_native_0027(int argc, char **argv);
int test_fh_byte_0001(int argc, char **argv);
int test_fh_byte_0002(int argc, char **argv);
int test_fh_byte_0003(int argc, char **argv);
int test_fh_byte_0004(int argc, char **argv);
int test_fh_byte_0005(int argc, char **argv);
int test_fh_byte_0006(int argc, char **argv);
int test_fh_byte_0007(int argc, char **argv);
int test_fh_byte_0008(int argc, char **argv);
int test_fh_weight_0001(int argc, char **argv);
int test_fh_weight_0002(int argc, char **argv);
int test_fh_weight_0003(int argc, char **argv);
int test_fh_weight_0004(int argc, char **argv);
int test_fh_weight_0005(int argc, char **argv);
int test_fh_weight_0006(int argc, char **argv);
int test_fh_weight_0007(int argc, char **argv);
int test_fh_weight_0008(int argc, char **argv);
int test_fh_weight_0009(int argc, char **argv);
int test_fh_weight_0010(int argc, char **argv);
int test_fh_weight_0011(int argc, char **argv);
int test_fh_weight_0012(int argc, char **argv);
int test_fh_weight_0013(int argc, char **argv);
int test_fh_weight_0014(int argc, char **argv);
int test_fh_weight_0015(int argc, char **argv);
int test_fh_weight_0016(int argc, char **argv);
int test_fh_weight_0017(int argc, char **argv);
int test_fh_weight_0018(int argc, char **argv);
int test_fh_weight_0019(int argc, char **argv);
int test_fh_weight_0020(int argc, char **argv);
int test_fh_weight_0021(int argc, char **argv);
int test_fh_weight_0022(int argc, char **argv);
int test_fh_weight_0023(int argc, char **argv);
int test_fh_weight_0024(int argc, char **argv);
int test_fh_weight_0025(int argc, char **argv);
int test_fh_weight_0026(int argc, char **argv);
int test_fh_weight_0027(int argc, char **argv);

int test_fh_lookup_0018(int argc, char **argv);
int test_fh_lookup_0019(int argc, char **argv);

int test_fh_lookup_0001(int argc, char **argv);
int test_fh_lookup_0002(int argc, char **argv);
int test_fh_lookup_0003(int argc, char **argv);
int test_fh_lookup_0004(int argc, char **argv);
int test_fh_lookup_0005(int argc, char **argv);
int test_fh_lookup_0006(int argc, char **argv);
int test_fh_lookup_0007(int argc, char **argv);
int test_fh_lookup_0008(int argc, char **argv);
int test_fh_lookup_0009(int argc, char **argv);
int test_fh_lookup_0010(int argc, char **argv);
int test_fh_lookup_0011(int argc, char **argv);
int test_fh_lookup_0012(int argc, char **argv);
int test_fh_lookup_0013(int argc, char **argv);
int test_fh_lookup_0014(int argc, char **argv);
int test_fh_lookup_0015(int argc, char **argv);
int test_fh_lookup_0016(int argc, char **argv);
int test_fh_lookup_0017(int argc, char **argv);
int test_fh_palette_0001(int argc, char **argv);
int test_fh_palette_0002(int argc, char **argv);
int test_fh_palette_0003(int argc, char **argv);
int test_fh_palette_0004(int argc, char **argv);
int test_fh_palette_0005(int argc, char **argv);
int test_fh_palette_0006(int argc, char **argv);
int test_fh_palette_0007(int argc, char **argv);
int test_fh_palette_0008(int argc, char **argv);
int test_fh_palette_0009(int argc, char **argv);
int test_fh_palette_0010(int argc, char **argv);
int test_fh_palette_0011(int argc, char **argv);
int test_fh_palette_0012(int argc, char **argv);
int test_eyt_lookup_0001(int argc, char **argv);
int test_eyt_lookup_0002(int argc, char **argv);
int test_eyt_lookup_0003(int argc, char **argv);
int test_eyt_lookup_0004(int argc, char **argv);
int test_eyt_lookup_0005(int argc, char **argv);
int test_eyt_lookup_0006(int argc, char **argv);
int test_eyt_lookup_0007(int argc, char **argv);
int test_eyt_lookup_0008(int argc, char **argv);
int test_eyt_lookup_0009(int argc, char **argv);
int test_eyt_lookup_0010(int argc, char **argv);
int test_eyt_lookup_0011(int argc, char **argv);
int test_eyt_lookup_0012(int argc, char **argv);
int test_eyt_lookup_0013(int argc, char **argv);
int test_eyt_lookup_0014(int argc, char **argv);
int test_eyt_lookup_0015(int argc, char **argv);
int test_eyt_lookup_0016(int argc, char **argv);
int test_eyt_lookup_0017(int argc, char **argv);
int test_eyt_lookup_0018(int argc, char **argv);
int test_eyt_lookup_0019(int argc, char **argv);
int test_eyt_lookup_0020(int argc, char **argv);
int test_eyt_lookup_0021(int argc, char **argv);
int test_eyt_lookup_0022(int argc, char **argv);
int test_eyt_lookup_0023(int argc, char **argv);
int test_eyt_lookup_0024(int argc, char **argv);
int test_eyt_lookup_0025(int argc, char **argv);
int test_eyt_lookup_0026(int argc, char **argv);
int test_eyt_lookup_0027(int argc, char **argv);
int test_eyt_lookup_0028(int argc, char **argv);
int test_eyt_lookup_0029(int argc, char **argv);
int test_eyt_lookup_0030(int argc, char **argv);
int test_eyt_lookup_0031(int argc, char **argv);
int test_eyt_lookup_0032(int argc, char **argv);
int test_eyt_lookup_0033(int argc, char **argv);
int test_eyt_lookup_0034(int argc, char **argv);
int test_eyt_lookup_0035(int argc, char **argv);
int test_eyt_lookup_0036(int argc, char **argv);
int test_eyt_lookup_0037(int argc, char **argv);
int test_eyt_lookup_0038(int argc, char **argv);
int test_eyt_palette_0001(int argc, char **argv);
int test_eyt_palette_0002(int argc, char **argv);
int test_eyt_palette_0003(int argc, char **argv);
int test_eyt_palette_0004(int argc, char **argv);
int test_eyt_palette_0005(int argc, char **argv);
int test_eyt_palette_0006(int argc, char **argv);
int test_eyt_palette_0007(int argc, char **argv);
int test_eyt_palette_0008(int argc, char **argv);
int test_eyt_palette_0009(int argc, char **argv);
int test_eyt_palette_0010(int argc, char **argv);
int test_eyt_palette_0011(int argc, char **argv);
int test_eyt_palette_0012(int argc, char **argv);
int test_mah_lookup_0001(int argc, char **argv);
int test_mah_lookup_0002(int argc, char **argv);
int test_mah_lookup_0003(int argc, char **argv);
int test_mah_lookup_0004(int argc, char **argv);
int test_mah_lookup_0005(int argc, char **argv);
int test_mah_lookup_0006(int argc, char **argv);
int test_mah_lookup_0007(int argc, char **argv);
int test_mah_lookup_0008(int argc, char **argv);
int test_mah_lookup_0009(int argc, char **argv);
int test_mah_lookup_0010(int argc, char **argv);
int test_mah_lookup_0011(int argc, char **argv);
int test_mah_lookup_0012(int argc, char **argv);
int test_mah_lookup_0013(int argc, char **argv);
int test_mah_lookup_0014(int argc, char **argv);
int test_mah_lookup_0015(int argc, char **argv);
int test_mah_lookup_0016(int argc, char **argv);
int test_mah_lookup_0017(int argc, char **argv);
int test_mah_lookup_0018(int argc, char **argv);
int test_mah_lookup_0019(int argc, char **argv);
int test_mah_lookup_0020(int argc, char **argv);
int test_mah_lookup_0021(int argc, char **argv);
int test_mah_lookup_0022(int argc, char **argv);
int test_mah_lookup_0023(int argc, char **argv);
int test_mah_lookup_0024(int argc, char **argv);
int test_mah_lookup_0025(int argc, char **argv);
int test_mah_lookup_0026(int argc, char **argv);
int test_mah_lookup_0027(int argc, char **argv);
int test_mah_lookup_0028(int argc, char **argv);
int test_mah_lookup_0029(int argc, char **argv);
int test_mah_lookup_0030(int argc, char **argv);
int test_mah_palette_0001(int argc, char **argv);
int test_mah_palette_0002(int argc, char **argv);
int test_mah_palette_0003(int argc, char **argv);
int test_mah_palette_0004(int argc, char **argv);
int test_mah_palette_0005(int argc, char **argv);
int test_mah_palette_0006(int argc, char **argv);
int test_mah_palette_0007(int argc, char **argv);
int test_mah_palette_0008(int argc, char **argv);
int test_mah_palette_0009(int argc, char **argv);
int test_mah_palette_0010(int argc, char **argv);
int test_mah_palette_0011(int argc, char **argv);
int test_mah_palette_0012(int argc, char **argv);
int test_lookup_palette(int argc, char **argv);
int test_palette_solvers_float(int argc, char **argv);
int test_srgb_0065(int argc, char **argv);
int test_srgb_0066(int argc, char **argv);
int test_srgb_0067(int argc, char **argv);
int test_srgb_0068(int argc, char **argv);
int test_palette_direct_float(int argc, char **argv);
int test_palette_retention_kcenter_boundary(int argc, char **argv);
int test_palette_retention_extended_read(int argc, char **argv);
int test_palette_retention_indexed_write(int argc, char **argv);
int test_snap_hooks_hook_disabled(int argc, char **argv);
int test_snap_hooks_policy_disabled(int argc, char **argv);
int test_snap_hooks_stage_excluded(int argc, char **argv);
int test_snap_hooks_rate_zero(int argc, char **argv);
int test_snap_hooks_early_nearest(int argc, char **argv);
int test_snap_hooks_early_reversible(int argc, char **argv);
int test_snap_hooks_final_nearest(int argc, char **argv);
int test_snap_hooks_final_reversible(int argc, char **argv);
int test_snap_hooks_large_source(int argc, char **argv);
int test_snap_hooks_double_blend(int argc, char **argv);
int test_snap_hooks_final_rollback(int argc, char **argv);
int test_snap_hooks_byte_grid(int argc, char **argv);
int test_snap_hooks_stage_ladder(int argc, char **argv);
int test_snap_hooks_kmeans_once(int argc, char **argv);
int test_snap_hooks_kmeans_all(int argc, char **argv);
int test_ordinary_center_sources(int argc, char **argv);
int test_ordinary_medoids_sources(int argc, char **argv);
int test_ordinary_center_hard(int argc, char **argv);
int test_ordinary_center_exact(int argc, char **argv);
int test_ordinary_center_ward(int argc, char **argv);
int test_ordinary_medoids_ward(int argc, char **argv);
int test_ordinary_center_snap(int argc, char **argv);
int test_ordinary_medoids_snap(int argc, char **argv);
int test_ordinary_center_fault(int argc, char **argv);
int test_ordinary_medoids_fault(int argc, char **argv);
int test_ordinary_center_nonfinite(int argc, char **argv);
int test_ordinary_medoids_nonfinite(int argc, char **argv);
int test_ordinary_center_hint(int argc, char **argv);
int test_ordinary_medoids_hint(int argc, char **argv);
int test_ordinary_center_byte(int argc, char **argv);
int test_ordinary_medoids_byte(int argc, char **argv);
int test_ordinary_center_reuse(int argc, char **argv);
int test_ordinary_medoids_reuse(int argc, char **argv);
int test_ordinary_center_reduce(int argc, char **argv);
int test_ordinary_medoids_reduce(int argc, char **argv);
int test_ordinary_kmeans_sources(int argc, char **argv);
int test_ordinary_kmeans_hard(int argc, char **argv);
int test_ordinary_kmeans_exact(int argc, char **argv);
int test_ordinary_kmeans_ward(int argc, char **argv);
int test_ordinary_kmeans_snap(int argc, char **argv);
int test_ordinary_kmeans_fault(int argc, char **argv);
int test_ordinary_kmeans_nonfinite(int argc, char **argv);
int test_ordinary_kmeans_hint(int argc, char **argv);
int test_ordinary_kmeans_byte(int argc, char **argv);
int test_ordinary_kmeans_reuse(int argc, char **argv);
int test_ordinary_kmeans_fractional(int argc, char **argv);
int test_ordinary_kmeans_polish(int argc, char **argv);
int test_ordinary_kmeans_empty(int argc, char **argv);
int test_ordinary_kmeans_feedback(int argc, char **argv);
int test_ordinary_kmeans_feedback_snap(int argc, char **argv);
int test_ordinary_kmeans_feedback_interval(int argc, char **argv);
int test_interframe_equal_pixels(int argc, char **argv);
int test_ordinary_kmeans_unique(int argc, char **argv);
int test_ordinary_heckbert_sources(int argc, char **argv);
int test_ordinary_heckbert_ward(int argc, char **argv);
int test_ordinary_heckbert_snap(int argc, char **argv);
int test_ordinary_heckbert_fault(int argc, char **argv);
int test_ordinary_heckbert_nonfinite(int argc, char **argv);
int test_ordinary_heckbert_hint(int argc, char **argv);
int test_ordinary_heckbert_byte(int argc, char **argv);
int test_ordinary_heckbert_reuse(int argc, char **argv);
int test_ordinary_heckbert_center(int argc, char **argv);
int test_ordinary_heckbert_average(int argc, char **argv);
int test_ordinary_heckbert_histogram(int argc, char **argv);
int test_ordinary_heckbert_polish(int argc, char **argv);
int test_ordinary_heckbert_once(int argc, char **argv);
int test_ordinary_heckbert_unique(int argc, char **argv);
int test_palette_retention_kmeans(int argc, char **argv);
int test_palette_retention_kcenter(int argc, char **argv);
int test_palette_retention_kmedoids(int argc, char **argv);
int test_palette_retention_typed(int argc, char **argv);
int test_palette_retention_getter_write(int argc, char **argv);
int test_palette_retention_setter_same(int argc, char **argv);
int test_palette_retention_wire(int argc, char **argv);
int test_palette_retention_cache(int argc, char **argv);
int test_palette_retention_byte(int argc, char **argv);
int test_palette_retention_invalid(int argc, char **argv);
int test_palette_retention_mono(int argc, char **argv);
int test_palette_retention_feedback(int argc, char **argv);
int test_palette_retention_heckbert(int argc, char **argv);
int test_palette_0051_p3_contract(int argc, char **argv);
int test_palette_0054_p3_encoder(int argc, char **argv);
int test_palette_0055_p3_mapfile(int argc, char **argv);
int test_palette_0059_p3_precision(int argc, char **argv);
int test_palette_0062_p3_fs_error(int argc, char **argv);
int test_security_0001_issue222_encoder_encode_bytes_packed_g1(int argc,
                                                                char **argv);

int test_loader_0008_loader_coregraphics_pixelformat(int argc, char **argv);
int test_loader_0009_loader_wic_pixelformat(int argc, char **argv);
int test_loader_0010_loader_gdk_pixbuf_pixelformat(int argc, char **argv);
int test_loader_0011_loader_gd_pixelformat(int argc, char **argv);
int test_loader_libpng_contract(int argc, char **argv);
int test_libpng_indexed_precision(int argc, char **argv);
int test_cg_expanded_values(int argc, char **argv);
int test_cgi_0001(int argc, char **argv);
int test_cgi_0002(int argc, char **argv);
int test_cgi_0003(int argc, char **argv);
int test_cgi_0004(int argc, char **argv);
int test_cgi_0005(int argc, char **argv);
int test_cgi_0006(int argc, char **argv);
int test_cgi_0007(int argc, char **argv);
int test_cgi_0008(int argc, char **argv);
int test_cgi_0009(int argc, char **argv);
int test_cgi_0010(int argc, char **argv);
int test_cgi_0011(int argc, char **argv);
int test_cicp_0063(int argc, char **argv);
int test_cicp_0064(int argc, char **argv);
int test_cicp_0065(int argc, char **argv);
int test_cicp_0066(int argc, char **argv);
int test_gifc_0030(int argc, char **argv);
int test_gifc_0031(int argc, char **argv);
int test_gifc_0032(int argc, char **argv);
int test_gifc_0033(int argc, char **argv);
int test_gifc_0034(int argc, char **argv);
int test_gifc_0035(int argc, char **argv);
int test_gifc_0036(int argc, char **argv);
int test_gifc_0037(int argc, char **argv);
int test_bmpc_0044(int argc, char **argv);
int test_bmpc_0045(int argc, char **argv);
int test_bmpc_0001(int argc, char **argv);
int test_bmpc_0002(int argc, char **argv);
int test_bmpc_0003(int argc, char **argv);
int test_bmpc_0004(int argc, char **argv);
int test_bmpc_0005(int argc, char **argv);
int test_bmpc_0006(int argc, char **argv);
int test_bmpc_0007(int argc, char **argv);
int test_bmpc_0008(int argc, char **argv);
int test_bmpc_0009(int argc, char **argv);
int test_bmpc_0010(int argc, char **argv);
int test_bmpc_0011(int argc, char **argv);
int test_bmpc_0012(int argc, char **argv);
int test_bmpc_0013(int argc, char **argv);
int test_bmpc_0014(int argc, char **argv);
int test_bmpc_0015(int argc, char **argv);
int test_bmpc_0016(int argc, char **argv);
int test_bmpc_0017(int argc, char **argv);
int test_bmpc_0018(int argc, char **argv);
int test_bmpc_0019(int argc, char **argv);
int test_bmpc_0020(int argc, char **argv);
int test_bmpc_0021(int argc, char **argv);
int test_bmpc_0022(int argc, char **argv);
int test_bmpc_0023(int argc, char **argv);
int test_bmpc_0024(int argc, char **argv);
int test_bmpc_0025(int argc, char **argv);
int test_bmpc_0026(int argc, char **argv);
int test_bmpc_0027(int argc, char **argv);
int test_bmpc_0028(int argc, char **argv);
int test_bmpc_0029(int argc, char **argv);
int test_bmpc_0030(int argc, char **argv);
int test_bmpc_0031(int argc, char **argv);
int test_bmpc_0032(int argc, char **argv);
int test_bmpc_0033(int argc, char **argv);
int test_bmpc_0034(int argc, char **argv);
int test_bmpc_0035(int argc, char **argv);
int test_bmpc_0036(int argc, char **argv);
int test_bmpc_0037(int argc, char **argv);
int test_bmpc_0038(int argc, char **argv);
int test_bmpc_0039(int argc, char **argv);
int test_bmpc_0040(int argc, char **argv);
int test_bmpc_0041(int argc, char **argv);
int test_bmpc_0042(int argc, char **argv);
int test_bmpc_0043(int argc, char **argv);
int test_gifc_0001(int argc, char **argv);
int test_gifc_0002(int argc, char **argv);
int test_gifc_0003(int argc, char **argv);
int test_gifc_0004(int argc, char **argv);
int test_gifc_0005(int argc, char **argv);
int test_gifc_0006(int argc, char **argv);
int test_gifc_0007(int argc, char **argv);
int test_gifc_0008(int argc, char **argv);
int test_gifc_0009(int argc, char **argv);
int test_gifc_0010(int argc, char **argv);
int test_gifc_0011(int argc, char **argv);
int test_gifc_0012(int argc, char **argv);
int test_gifc_0013(int argc, char **argv);
int test_gifc_0014(int argc, char **argv);
int test_gifc_0015(int argc, char **argv);
int test_gifc_0016(int argc, char **argv);
int test_gifc_0017(int argc, char **argv);
int test_gifc_0018(int argc, char **argv);
int test_gifc_0019(int argc, char **argv);
int test_gifc_0020(int argc, char **argv);
int test_gifc_0021(int argc, char **argv);
int test_gifc_0022(int argc, char **argv);
int test_gifc_0023(int argc, char **argv);
int test_gifc_0024(int argc, char **argv);
int test_gifc_0025(int argc, char **argv);
int test_gifc_0026(int argc, char **argv);
int test_gifc_0027(int argc, char **argv);
int test_gifc_0028(int argc, char **argv);
int test_gifc_0029(int argc, char **argv);
int test_cicp_0001(int argc, char **argv);
int test_cicp_0002(int argc, char **argv);
int test_cicp_0003(int argc, char **argv);
int test_cicp_0004(int argc, char **argv);
int test_cicp_0005(int argc, char **argv);
int test_cicp_0006(int argc, char **argv);
int test_cicp_0007(int argc, char **argv);
int test_cicp_0008(int argc, char **argv);
int test_cicp_0009(int argc, char **argv);
int test_cicp_0010(int argc, char **argv);
int test_cicp_0011(int argc, char **argv);
int test_cicp_0012(int argc, char **argv);
int test_cicp_0013(int argc, char **argv);
int test_cicp_0014(int argc, char **argv);
int test_cicp_0015(int argc, char **argv);
int test_cicp_0016(int argc, char **argv);
int test_cicp_0017(int argc, char **argv);
int test_cicp_0018(int argc, char **argv);
int test_cicp_0019(int argc, char **argv);
int test_cicp_0020(int argc, char **argv);
int test_cicp_0021(int argc, char **argv);
int test_cicp_0022(int argc, char **argv);
int test_cicp_0023(int argc, char **argv);
int test_cicp_0024(int argc, char **argv);
int test_cicp_0025(int argc, char **argv);
int test_cicp_0026(int argc, char **argv);
int test_cicp_0027(int argc, char **argv);
int test_cicp_0028(int argc, char **argv);
int test_cicp_0029(int argc, char **argv);
int test_cicp_0030(int argc, char **argv);
int test_cicp_0031(int argc, char **argv);
int test_cicp_0032(int argc, char **argv);
int test_cicp_0033(int argc, char **argv);
int test_cicp_0034(int argc, char **argv);
int test_cicp_0035(int argc, char **argv);
int test_cicp_0036(int argc, char **argv);
int test_cicp_0037(int argc, char **argv);
int test_cicp_0038(int argc, char **argv);
int test_cicp_0039(int argc, char **argv);
int test_cicp_0040(int argc, char **argv);
int test_cicp_0041(int argc, char **argv);
int test_cicp_0042(int argc, char **argv);
int test_cicp_0043(int argc, char **argv);
int test_cicp_0044(int argc, char **argv);
int test_cicp_0045(int argc, char **argv);
int test_cicp_0046(int argc, char **argv);
int test_cicp_0047(int argc, char **argv);
int test_cicp_0048(int argc, char **argv);
int test_cicp_0049(int argc, char **argv);
int test_cicp_0050(int argc, char **argv);
int test_cicp_0051(int argc, char **argv);
int test_cicp_0052(int argc, char **argv);
int test_cicp_0053(int argc, char **argv);
int test_cicp_0054(int argc, char **argv);
int test_cicp_0055(int argc, char **argv);
int test_cicp_0056(int argc, char **argv);
int test_cicp_0057(int argc, char **argv);
int test_cicp_0058(int argc, char **argv);
int test_cicp_0059(int argc, char **argv);
int test_cicp_0060(int argc, char **argv);
int test_cicp_0061(int argc, char **argv);
int test_cicp_0062(int argc, char **argv);
int test_tgac_0001(int argc, char **argv);
int test_tgac_0002(int argc, char **argv);
int test_tgac_0003(int argc, char **argv);
int test_tgac_0004(int argc, char **argv);
int test_tgac_0005(int argc, char **argv);
int test_tgac_0006(int argc, char **argv);
int test_tgac_0007(int argc, char **argv);
int test_tgac_0008(int argc, char **argv);
int test_tgac_0009(int argc, char **argv);
int test_tgac_0010(int argc, char **argv);
int test_tgac_0011(int argc, char **argv);
int test_tgac_0012(int argc, char **argv);
int test_tgac_0013(int argc, char **argv);
int test_tgac_0014(int argc, char **argv);
int test_tgac_0015(int argc, char **argv);
int test_tgac_0016(int argc, char **argv);
int test_tgac_0017(int argc, char **argv);
int test_tgac_0018(int argc, char **argv);
int test_tgac_0019(int argc, char **argv);
int test_tgac_0020(int argc, char **argv);
int test_tgac_0021(int argc, char **argv);
int test_tgac_0022(int argc, char **argv);
int test_tgac_0023(int argc, char **argv);
int test_tgac_0024(int argc, char **argv);
int test_tgac_0025(int argc, char **argv);
int test_tgac_0026(int argc, char **argv);
int test_tgac_0027(int argc, char **argv);
int test_tgac_0028(int argc, char **argv);
int test_tgac_0029(int argc, char **argv);
int test_tgac_0030(int argc, char **argv);
int test_tgac_0031(int argc, char **argv);
int test_tgac_0032(int argc, char **argv);
int test_tgac_0033(int argc, char **argv);
int test_tgac_0034(int argc, char **argv);
int test_tgac_0035(int argc, char **argv);
int test_tgac_0036(int argc, char **argv);
int test_tgac_0037(int argc, char **argv);
int test_tgac_0038(int argc, char **argv);
int test_bjicc_0001(int argc, char **argv);
int test_bjicc_0002(int argc, char **argv);
int test_bjicc_0003(int argc, char **argv);
int test_bjicc_0004(int argc, char **argv);
int test_bjicc_0005(int argc, char **argv);
int test_bjicc_0006(int argc, char **argv);
int test_bjicc_0007(int argc, char **argv);
int test_bjicc_0008(int argc, char **argv);
int test_bjicc_0009(int argc, char **argv);
int test_bjicc_0010(int argc, char **argv);
int test_bjicc_0011(int argc, char **argv);
int test_bjicc_0012(int argc, char **argv);
int test_bjicc_0013(int argc, char **argv);
int test_bjicc_0014(int argc, char **argv);
int test_bjicc_0015(int argc, char **argv);
int test_bjicc_0016(int argc, char **argv);
int test_bjicc_0017(int argc, char **argv);
int test_bjicc_0018(int argc, char **argv);
int test_bjicc_0019(int argc, char **argv);
int test_bjicc_0020(int argc, char **argv);
int test_bjicc_0021(int argc, char **argv);
int test_bjicc_0022(int argc, char **argv);
int test_bjicc_0023(int argc, char **argv);
int test_bjicc_0024(int argc, char **argv);
int test_bjicc_0025(int argc, char **argv);
int test_bjicc_0026(int argc, char **argv);
int test_bjicc_0027(int argc, char **argv);
int test_bjicc_0028(int argc, char **argv);
int test_bjicc_0029(int argc, char **argv);
int test_bjicc_0030(int argc, char **argv);
int test_bjicc_0031(int argc, char **argv);
int test_bjicc_0032(int argc, char **argv);
int test_bjicc_0033(int argc, char **argv);
int test_bjicc_0034(int argc, char **argv);
int test_bjicc_0035(int argc, char **argv);
int test_bjicc_0036(int argc, char **argv);
int test_bjicc_0037(int argc, char **argv);
int test_bjicc_0038(int argc, char **argv);
int test_bjicc_0039(int argc, char **argv);
int test_bjicc_0040(int argc, char **argv);
int test_bjicc_0041(int argc, char **argv);
int test_bjicc_0042(int argc, char **argv);
int test_bjicc_0043(int argc, char **argv);
int test_bjicc_0044(int argc, char **argv);
int test_bjicc_0045(int argc, char **argv);
int test_bjicc_0046(int argc, char **argv);
int test_bjicc_0047(int argc, char **argv);
int test_bjicc_0048(int argc, char **argv);
int test_bjicc_0049(int argc, char **argv);
int test_bjicc_0050(int argc, char **argv);
int test_bjicc_0051(int argc, char **argv);
int test_bjicc_0052(int argc, char **argv);
int test_bjicc_0053(int argc, char **argv);
int test_jgray_0001(int argc, char **argv);
int test_jgray_0002(int argc, char **argv);
int test_jgray_0003(int argc, char **argv);
int test_jgray_0004(int argc, char **argv);
int test_jgray_0005(int argc, char **argv);
int test_jgray_0006(int argc, char **argv);
int test_jgray_0007(int argc, char **argv);
int test_jgray_0008(int argc, char **argv);
int test_jgray_0009(int argc, char **argv);
int test_jgray_0010(int argc, char **argv);
int test_jgray_0011(int argc, char **argv);
int test_jgray_0012(int argc, char **argv);
int test_jgray_0013(int argc, char **argv);
int test_jgray_0014(int argc, char **argv);
int test_jgray_0015(int argc, char **argv);
int test_jgray_0016(int argc, char **argv);
int test_loader_0012_loader_libpng_pixelformat(int argc, char **argv);
int test_loader_0013_loader_libjpeg_pixelformat(int argc, char **argv);
int test_jp_gray16(int argc, char **argv);
int test_jp_fault(int argc, char **argv);
int test_jc_precision(int argc, char **argv);
int test_jc_0001(int argc, char **argv);
int test_jc_0002(int argc, char **argv);
int test_jc_0003(int argc, char **argv);
int test_jc_0004(int argc, char **argv);
int test_jc_0005(int argc, char **argv);
int test_jc_0006(int argc, char **argv);
int test_jc_0007(int argc, char **argv);
int test_jc_0008(int argc, char **argv);
int test_jc_0009(int argc, char **argv);
int test_jc_0010(int argc, char **argv);
int test_jc_0011(int argc, char **argv);
int test_jc_0012(int argc, char **argv);
int test_jc_0013(int argc, char **argv);
int test_jc_0014(int argc, char **argv);
int test_jc_0015(int argc, char **argv);
int test_jc_0016(int argc, char **argv);
int test_jc_0017(int argc, char **argv);
int test_jc_0018(int argc, char **argv);
int test_jc_0019(int argc, char **argv);
int test_loader_0014_loader_builtin_pixelformat(int argc, char **argv);
int test_loader_0015_loader_quicklook_pixelformat(int argc, char **argv);
int test_loader_0016_loader_gnome_thumbnailer_pixelformat(int argc,
                                                          char **argv);
int test_aborttrace_0001_img2sixel_aborttrace(int argc, char **argv);
int test_loader_0017_loader_libwebp_pixelformat(int argc, char **argv);
int test_loader_0018_loader_libwebp_animation_frames(int argc, char **argv);
int test_loader_0019_loader_libwebp_palette_promotion_guard(int argc,
                                                             char **argv);
int test_loader_0024_loader_libwebp_fault_demux(int argc, char **argv);
int test_loader_0026_loader_libwebp_fault_options_init(int argc, char **argv);
int test_loader_0027_loader_libwebp_fault_decoder_new(int argc, char **argv);
int test_loader_0028_loader_libwebp_fault_decoder_getinfo(int argc,
                                                          char **argv);
int test_loader_0029_loader_libwebp_fault_decoder_getnext(int argc,
                                                          char **argv);
int test_loader_0032_loader_libwebp_fault_static_rgbinto(int argc,
                                                         char **argv);
int test_loader_0033_loader_libwebp_fault_static_rgbainto(int argc,
                                                          char **argv);
int test_loader_0034_loader_libwebp_fault_lossy_init_config(int argc,
                                                             char **argv);
int test_loader_0035_loader_libwebp_fault_lossy_decode(int argc, char **argv);
int test_loader_0036_loader_libwebp_fault_lossy_yuv_plane_missing(int argc,
                                                                   char **argv);
int test_loader_0037_loader_libwebp_fault_lossy_yuv_stride_invalid(int argc,
                                                                    char **argv);
int test_loader_0038_loader_libwebp_fault_lossy_dimensions_mismatch(int argc,
                                                                     char **argv);
int test_loader_0039_loader_libwebp_fault_no_frames(int argc, char **argv);
int test_loader_0040_loader_libwebp_fault_no_anmf(int argc, char **argv);
int test_loader_0041_loader_libwebp_frame_count_limit_fast(int argc,
                                                           char **argv);
int test_loader_0042_loader_libwebp_frame_count_limit_decoder_guard(int argc,
                                                                     char **argv);
int test_loader_0043_loader_libwebp_fault_get_features_static(int argc,
                                                              char **argv);
int test_loader_0044_loader_libwebp_fault_get_features_animation(int argc,
                                                                 char **argv);
int test_loader_0045_loader_libwebp_fault_static_malloc(int argc,
                                                        char **argv);
int test_loader_0046_loader_libwebp_fault_animation_canvas_malloc(int argc,
                                                                  char **argv);
int test_loader_0047_loader_libwebp_fault_lossy_malloc(int argc,
                                                       char **argv);
int test_loader_0048_loader_libwebp_fault_anmf_payload_too_small(int argc,
                                                                 char **argv);
int test_loader_0049_loader_libwebp_fault_anmf_extract_malloc(int argc,
                                                              char **argv);
int test_loader_0050_loader_osc11_colorspec_parser(int argc, char **argv);
int test_loader_0051_loader_osc11_response_parser(int argc, char **argv);
int test_loader_0052_loader_osc11_query_control(int argc, char **argv);
int test_loader_0053_loader_background_colorspace_override(int argc,
                                                            char **argv);
int test_loader_0054_loader_gif_bgcolor_canvas_fill(int argc, char **argv);
int test_loader_0055_loader_animation_hide_cursor_control(int argc,
                                                           char **argv);
int test_loader_0056_loader_factory_predicate_gate(int argc, char **argv);
int test_loader_0057_loader_gd_can_try_policy(int argc, char **argv);
int test_loader_0058_loader_gd_status_policy(int argc, char **argv);
int test_loader_0059_loader_gd_pixelpolicy_detail(int argc, char **argv);
int test_loader_0061_loader_tty_cpr_parser(int argc, char **argv);
int test_loader_0062_loader_animation_hide_cursor_environment(int argc,
                                                               char **argv);
int test_loader_0063_loader_builtin_rgba_alpha_mask(int argc, char **argv);
int test_loader_0064_loader_tty_response_dispatcher(int argc, char **argv);
int test_loader_0065_loader_osc11_colorspec_reject(int argc, char **argv);
int test_loader_0066_loader_osc11_response_reject(int argc, char **argv);
int test_loader_0067_loader_wait_for_condition(int argc, char **argv);
int test_loader_0068_loader_background_colorspace_gamma(int argc,
                                                         char **argv);
int test_loader_0069_loader_background_colorspace_reset(int argc,
                                                         char **argv);
int test_loader_0070_loader_gif_bgcolor_canvas_fill_explicit_first(
    int argc,
    char **argv);
int test_loader_0071_loader_builtin_gif87a_numeric(int argc, char **argv);
int test_loader_0072_loader_builtin_gif_interlace_four_pass_numeric(
    int argc,
    char **argv);
int test_loader_0073_loader_builtin_gif_rectangle_offset_numeric(
    int argc,
    char **argv);
int test_loader_0074_loader_builtin_gif_rectangle_oob_reject(
    int argc,
    char **argv);
int test_loader_0075_loader_builtin_gif_extension_subblocks_numeric(
    int argc,
    char **argv);
int test_loader_0076_loader_builtin_gif_truncated_raster_reject(
    int argc,
    char **argv);
int test_loader_0077_loader_builtin_gif_illegal_lzw_code_reject(
    int argc,
    char **argv);
int test_loader_0078_loader_builtin_gif_disposal2_numeric(
    int argc,
    char **argv);
int test_loader_0079_loader_builtin_gif_lzw_12bit_width_numeric(
    int argc,
    char **argv);
int test_loader_0080_loader_builtin_tga_truecolor16_numeric(
    int argc,
    char **argv);
int test_loader_0081_loader_builtin_tga_palette8_numeric(
    int argc,
    char **argv);
int test_loader_0082_loader_builtin_tga_palette16_numeric(
    int argc,
    char **argv);
int test_loader_0083_loader_builtin_tga_index16_numeric(
    int argc,
    char **argv);
int test_loader_0084_loader_builtin_tga_bottom_origin_numeric(
    int argc,
    char **argv);
int test_loader_0085_loader_builtin_tga_rle_packet_modes_numeric(
    int argc,
    char **argv);
int test_loader_0086_loader_builtin_tga_truncated_palette_reject(
    int argc,
    char **argv);
int test_loader_0087_loader_builtin_tga_oob_index_fallback_numeric(
    int argc,
    char **argv);
int test_loader_0088_loader_builtin_pic_omitted_channels_numeric(
    int argc,
    char **argv);
int test_loader_0089_pic_repeated_channel_numeric(
    int argc,
    char **argv);
int test_loader_0090_pic_pure_rle_clip_numeric(
    int argc,
    char **argv);
int test_loader_0091_loader_builtin_pic_mixed_rle_raw128_numeric(
    int argc,
    char **argv);
int test_loader_0092_loader_builtin_pic_mixed_rle_repeat128_numeric(
    int argc,
    char **argv);
int test_loader_0093_loader_builtin_pic_ten_packet_limit_numeric(
    int argc,
    char **argv);
int test_loader_0094_builtin_png_apng_dispose_previous_numeric(
    int argc,
    char **argv);
int test_loader_0095_loader_builtin_jpeg_ycck8_rgb_digest(
    int argc,
    char **argv);
int test_loader_0096_builtin_psd_missing_composite_rgb8_digest(
    int argc,
    char **argv);
int test_loader_0098_loader_builtin_pic_raw_rgb_numeric(
    int argc,
    char **argv);
int test_loader_0099_loader_builtin_pic_mixed_rle_extended_numeric(
    int argc,
    char **argv);
int test_loader_0100_loader_builtin_tga_truecolor24_numeric(
    int argc,
    char **argv);
int test_loader_0101_loader_builtin_tga_grayscale8_numeric(
    int argc,
    char **argv);
int test_loader_0102_loader_builtin_pic_chained_packets_numeric(
    int argc,
    char **argv);
int test_loader_0103_loader_builtin_gif_disposal3_numeric(
    int argc,
    char **argv);
int test_loader_0104_loader_builtin_gif_local_palette_numeric(
    int argc,
    char **argv);
int test_loader_0105_loader_builtin_hdr_new_rle_numeric(
    int argc,
    char **argv);
int test_loader_0106_loader_builtin_hdr_old_scanline_rle_numeric(
    int argc,
    char **argv);
int test_loader_0107_loader_builtin_hdr_legacy_stream_numeric(
    int argc,
    char **argv);
int test_loader_0108_loader_builtin_jpeg_rgb8_sequential_digest(
    int argc,
    char **argv);
int test_loader_0109_loader_builtin_png_icc_numeric(int argc, char **argv);
int test_loader_0110_loader_builtin_jpeg_icc_numeric(int argc, char **argv);
int test_loader_0111_loader_builtin_psd_icc_digest(int argc, char **argv);
int test_loader_0115_loader_builtin_jpeg_rgb16_lossless_numeric(
    int argc,
    char **argv);
int test_loader_0116_loader_builtin_psd_rgb16_zip_pred_numeric(
    int argc,
    char **argv);
int test_loader_0117_loader_builtin_psd_rgb32_rle_numeric(
    int argc,
    char **argv);
int test_loader_0118_loader_builtin_sixel_unpainted_numeric(
    int argc,
    char **argv);
int test_loader_0119_loader_builtin_sixel_high_color_numeric(
    int argc,
    char **argv);
int test_loader_0120_loader_builtin_pic_zero_dimension_reject(
    int argc,
    char **argv);
int test_loader_0121_loader_builtin_pic_eleventh_packet_reject(
    int argc,
    char **argv);
int test_loader_0122_loader_builtin_tga_truncated_direct_reject(
    int argc,
    char **argv);
int test_loader_0123_loader_builtin_tga_rle_overrun_reject(
    int argc,
    char **argv);
int test_loader_0124_loader_manager_open_plan(int argc, char **argv);
int test_loader_0125_loader_manager_closed_plan(int argc, char **argv);
int test_loader_0126_loader_manager_fallback_status(int argc, char **argv);
int test_loader_0127_loader_builtin_gif_animation_metadata_numeric(
    int argc,
    char **argv);
int test_loader_0128_loader_builtin_apng_animation_metadata_numeric(
    int argc,
    char **argv);
int test_loader_0130_loader_builtin_hdr_new_rle_zero_count_reject(
    int argc,
    char **argv);
int test_loader_0131_loader_builtin_hdr_new_rle_overrun_reject(
    int argc,
    char **argv);
int test_loader_0132_loader_builtin_hdr_new_rle_truncated_reject(
    int argc,
    char **argv);
int test_loader_0133_loader_builtin_hdr_legacy_repeat_first_reject(
    int argc,
    char **argv);
int test_loader_0134_hdr_legacy_repeat_overrun_reject(
    int argc,
    char **argv);
int test_loader_0135_loader_builtin_png_filter_modes_numeric(
    int argc,
    char **argv);
int test_loader_0136_loader_builtin_png_adam7_indexed4_digest(
    int argc,
    char **argv);
int test_loader_0137_loader_builtin_png_gray16_sub8bit_numeric(
    int argc,
    char **argv);
int test_loader_0138_loader_builtin_apng_blend_over_numeric(
    int argc,
    char **argv);
int test_loader_0139_loader_builtin_apng_dispose_background_numeric(
    int argc,
    char **argv);
int test_loader_0140_loader_builtin_jpeg_gray16_sub8bit_numeric(
    int argc,
    char **argv);
int test_loader_0141_loader_builtin_jpeg_progressive_420_digest(
    int argc,
    char **argv);
int test_loader_0142_loader_builtin_jpeg_gray16_restart_numeric(
    int argc,
    char **argv);
int test_loader_0143_loader_builtin_jpeg_ycbcr444_digest(
    int argc,
    char **argv);
int test_loader_0144_loader_builtin_jpeg_cmyk8_digest(
    int argc,
    char **argv);
int test_loader_0150_loader_builtin_psd_rgb8_alpha_mask_digest(
    int argc,
    char **argv);
int test_loader_0151_loader_builtin_psd_rgb8_alpha_white_digest(
    int argc,
    char **argv);
int test_loader_0152_loader_builtin_psd_cmyk16_zip_pred_numeric(
    int argc,
    char **argv);
int test_loader_0153_loader_builtin_psd_lab32_zip_pred_numeric(
    int argc,
    char **argv);
int test_loader_0154_loader_builtin_psb_layer_info_u64max_reject(
    int argc,
    char **argv);
int test_loader_0155_loader_builtin_allocator_failure_matrix(
    int argc,
    char **argv);
int test_loader_0156_loader_builtin_callback_status_matrix(
    int argc,
    char **argv);
int test_loader_0157_loader_builtin_png_cgbi_numeric(
    int argc,
    char **argv);
int test_loader_0158_loader_builtin_psd_blend_modes_numeric(
    int argc,
    char **argv);
int test_loader_0159_loader_builtin_psd_clipping_digest(
    int argc,
    char **argv);
int test_loader_0160_loader_builtin_psd_mask_digest(
    int argc,
    char **argv);
int test_loader_0161_loader_builtin_psd_effect_digest(
    int argc,
    char **argv);
int test_loader_0162_loader_builtin_apng_cancel_boundaries(
    int argc,
    char **argv);
int test_loader_0164_loader_builtin_jpeg_lossless_preds_numeric(
    int argc,
    char **argv);
int test_loader_0165_loader_builtin_jpeg_rgb_restart_numeric(
    int argc,
    char **argv);
int test_loader_0166_loader_builtin_apng_allocation_failures(
    int argc,
    char **argv);
int test_loader_0168_loader_builtin_psd_effect_allocation_failures(
    int argc,
    char **argv);
int test_loader_0169_loader_builtin_png_icc_allocation_failures(
    int argc,
    char **argv);
int test_loader_0173_loader_builtin_tga_header_collision_reject(
    int argc,
    char **argv);
int test_loader_0174_loader_builtin_png_chrm_gama_float_numeric(
    int argc,
    char **argv);
int test_loader_0175_loader_builtin_png_singular_chrm_numeric(
    int argc,
    char **argv);
int test_loader_0176_loader_builtin_png_bkgd_chrm_numeric(
    int argc,
    char **argv);
int test_loader_0177_png_post_idat_actl_static(
    int argc,
    char **argv);
int test_loader_0178_apng_default_excluded(
    int argc,
    char **argv);
int test_loader_0179_apng_early_static_fallback(
    int argc,
    char **argv);
int test_loader_0180_apng_late_count_reject(
    int argc,
    char **argv);
int test_loader_0181_apng_late_sequence_reject(
    int argc,
    char **argv);
int test_loader_0182_apng_callback_no_fallback(
    int argc,
    char **argv);
int test_loader_0183_jpeg_source_numeric(int argc, char **argv);
int test_loader_0184_psd_cmyk_icc_numeric(int argc, char **argv);
int test_psd_color_0001(int argc, char **argv);
int test_psd_color_0002(int argc, char **argv);
int test_psd_color_0003(int argc, char **argv);
int test_psd_color_0004(int argc, char **argv);
int test_psd_color_0005(int argc, char **argv);
int test_psd_color_0006(int argc, char **argv);
int test_psd_color_0007(int argc, char **argv);
int test_psd_color_0008(int argc, char **argv);
int test_psd_color_0009(int argc, char **argv);
int test_psd_color_0010(int argc, char **argv);
int test_psd_color_0011(int argc, char **argv);
int test_psd_color_0012(int argc, char **argv);
int test_psd_color_0013(int argc, char **argv);
int test_psd_color_0014(int argc, char **argv);
int test_psd_color_0015(int argc, char **argv);
int test_psd_color_0016(int argc, char **argv);
int test_psd_color_0017(int argc, char **argv);
int test_psd_color_0018(int argc, char **argv);
int test_psd_color_0019(int argc, char **argv);
int test_psd_color_0020(int argc, char **argv);
int test_psd_color_0021(int argc, char **argv);
int test_psd_color_0022(int argc, char **argv);
int test_psd_color_0023(int argc, char **argv);
int test_psd_color_0024(int argc, char **argv);
int test_psd_color_0025(int argc, char **argv);
int test_psd_color_0026(int argc, char **argv);
int test_psd_color_0027(int argc, char **argv);
int test_psd_color_0028(int argc, char **argv);
int test_psd_color_0029(int argc, char **argv);
int test_psd_color_0030(int argc, char **argv);
int test_psd_color_0031(int argc, char **argv);
int test_psd_color_0032(int argc, char **argv);
int test_psd_color_0033(int argc, char **argv);
int test_psd_color_0034(int argc, char **argv);
int test_psd_color_0035(int argc, char **argv);
int test_psd_color_0036(int argc, char **argv);
int test_psd_color_0037(int argc, char **argv);
int test_psd_color_0038(int argc, char **argv);
int test_psd_color_0039(int argc, char **argv);
int test_psd_color_0040(int argc, char **argv);
int test_psd_color_0041(int argc, char **argv);
int test_psd_color_0042(int argc, char **argv);
int test_psd_color_0043(int argc, char **argv);
int test_psd_color_0044(int argc, char **argv);
int test_psd_color_0045(int argc, char **argv);
int test_psd_color_0046(int argc, char **argv);
int test_psd_color_0047(int argc, char **argv);
int test_psd_color_0048(int argc, char **argv);
int test_psd_color_0049(int argc, char **argv);
int test_psd_color_0050(int argc, char **argv);
int test_psd_color_0051(int argc, char **argv);
int test_psd_color_0052(int argc, char **argv);
int test_psd_color_0053(int argc, char **argv);
int test_psd_color_0054(int argc, char **argv);
int test_psd_color_0055(int argc, char **argv);
int test_psd_color_0056(int argc, char **argv);
int test_psd_color_0057(int argc, char **argv);
int test_psd_color_0058(int argc, char **argv);
int test_psd_color_0059(int argc, char **argv);
int test_psd_color_0060(int argc, char **argv);
int test_psd_color_0061(int argc, char **argv);
int test_psd_color_0062(int argc, char **argv);
int test_psd_color_0063(int argc, char **argv);
int test_psd_color_0064(int argc, char **argv);
int test_psd_color_0065(int argc, char **argv);
int test_psd_color_0066(int argc, char **argv);
int test_psd_color_0067(int argc, char **argv);
int test_psd_color_0068(int argc, char **argv);
int test_psd_color_0069(int argc, char **argv);
int test_psd_color_0070(int argc, char **argv);
int test_psd_color_0071(int argc, char **argv);
int test_psd_color_0072(int argc, char **argv);
int test_psd_color_0073(int argc, char **argv);
int test_psd_color_0074(int argc, char **argv);
int test_psd_color_0075(int argc, char **argv);
int test_psd_color_0076(int argc, char **argv);
int test_psd_color_0077(int argc, char **argv);
int test_psd_color_0078(int argc, char **argv);
int test_psd_color_0079(int argc, char **argv);
int test_psd_color_0080(int argc, char **argv);
int test_psd_color_0081(int argc, char **argv);
int test_psd_color_0082(int argc, char **argv);
int test_psd_color_0083(int argc, char **argv);
int test_psd_color_0084(int argc, char **argv);
int test_psd_color_0085(int argc, char **argv);
int test_psd_color_0086(int argc, char **argv);
int test_psd_color_0087(int argc, char **argv);
int test_psd_color_0088(int argc, char **argv);
int test_psd_color_0089(int argc, char **argv);
int test_psd_color_0090(int argc, char **argv);
int test_psd_color_0091(int argc, char **argv);
int test_psd_color_0092(int argc, char **argv);
int test_psd_color_0093(int argc, char **argv);
int test_psd_color_0094(int argc, char **argv);
int test_psd_color_0095(int argc, char **argv);
int test_psd_color_0096(int argc, char **argv);
int test_psd_color_0097(int argc, char **argv);
int test_psd_color_0098(int argc, char **argv);
int test_psd_color_0099(int argc, char **argv);
int test_psd_color_0100(int argc, char **argv);
int test_psd_color_0101(int argc, char **argv);
int test_psd_color_0102(int argc, char **argv);
int test_psd_color_0103(int argc, char **argv);
int test_psd_color_0104(int argc, char **argv);
int test_psd_color_0105(int argc, char **argv);
int test_psd_color_0106(int argc, char **argv);
int test_psd_color_0107(int argc, char **argv);
int test_psd_color_0108(int argc, char **argv);
int test_psd_color_0109(int argc, char **argv);
int test_psd_color_0110(int argc, char **argv);
int test_psd_color_0111(int argc, char **argv);
int test_psd_color_0112(int argc, char **argv);
int test_psd_color_0113(int argc, char **argv);
int test_psd_color_0114(int argc, char **argv);
int test_psd_color_0115(int argc, char **argv);
int test_psd_color_0116(int argc, char **argv);
int test_psd_color_0117(int argc, char **argv);
int test_psd_color_0118(int argc, char **argv);
int test_psd_color_0119(int argc, char **argv);
int test_psd_color_0120(int argc, char **argv);
int test_psd_color_0121(int argc, char **argv);
int test_psd_color_0122(int argc, char **argv);
int test_psd_color_0123(int argc, char **argv);
int test_psd_color_0124(int argc, char **argv);
int test_psd_color_0125(int argc, char **argv);
int test_psd_color_0126(int argc, char **argv);
int test_psd_color_0127(int argc, char **argv);
int test_psd_color_0128(int argc, char **argv);
int test_psd_color_0129(int argc, char **argv);
int test_psd_color_0130(int argc, char **argv);
int test_psd_color_0131(int argc, char **argv);
int test_psd_color_0132(int argc, char **argv);
int test_psd_color_0133(int argc, char **argv);
int test_psd_color_0134(int argc, char **argv);
int test_psd_color_0135(int argc, char **argv);
int test_psd_color_0136(int argc, char **argv);
int test_psd_color_0137(int argc, char **argv);
int test_psd_color_0138(int argc, char **argv);
int test_psd_color_0139(int argc, char **argv);
int test_psd_color_0140(int argc, char **argv);
int test_psd_color_0141(int argc, char **argv);
int test_psd_color_0142(int argc, char **argv);
int test_psd_color_0143(int argc, char **argv);
int test_psd_color_0144(int argc, char **argv);
int test_psd_color_0145(int argc, char **argv);
int test_psd_color_0146(int argc, char **argv);
int test_psd_color_0147(int argc, char **argv);
int test_psd_color_0148(int argc, char **argv);
int test_psd_color_0149(int argc, char **argv);
int test_psd_color_0150(int argc, char **argv);
int test_psd_color_0151(int argc, char **argv);
int test_psd_color_0152(int argc, char **argv);
int test_psd_color_0153(int argc, char **argv);
int test_psd_color_0154(int argc, char **argv);
int test_psd_color_0155(int argc, char **argv);
int test_psd_color_0156(int argc, char **argv);
int test_psd_color_0157(int argc, char **argv);
int test_psd_color_0158(int argc, char **argv);
int test_psd_color_0159(int argc, char **argv);
int test_psd_color_0160(int argc, char **argv);
int test_psd_color_0161(int argc, char **argv);
int test_psd_color_0162(int argc, char **argv);
int test_psd_color_0163(int argc, char **argv);
int test_psd_color_0164(int argc, char **argv);
int test_psd_color_0165(int argc, char **argv);
int test_psd_color_0166(int argc, char **argv);
int test_psd_color_0167(int argc, char **argv);
int test_psd_color_0168(int argc, char **argv);
int test_psd_color_0169(int argc, char **argv);
int test_psd_color_0170(int argc, char **argv);
int test_psd_color_0171(int argc, char **argv);
int test_psd_color_0172(int argc, char **argv);
int test_psd_color_0173(int argc, char **argv);
int test_psd_color_0174(int argc, char **argv);
int test_psd_color_0175(int argc, char **argv);
int test_psd_color_0176(int argc, char **argv);
int test_psd_color_0177(int argc, char **argv);
int test_psd_color_0178(int argc, char **argv);
int test_psd_color_0179(int argc, char **argv);
int test_psd_color_0180(int argc, char **argv);
int test_psd_color_0181(int argc, char **argv);
int test_psd_color_0182(int argc, char **argv);
int test_psd_color_0183(int argc, char **argv);
int test_psd_color_0184(int argc, char **argv);
int test_psd_color_0185(int argc, char **argv);
int test_psd_color_0186(int argc, char **argv);
int test_psd_color_0187(int argc, char **argv);
int test_psd_color_0188(int argc, char **argv);
int test_psd_color_0189(int argc, char **argv);
int test_psd_color_0190(int argc, char **argv);
int test_psd_color_0191(int argc, char **argv);
int test_psd_color_0192(int argc, char **argv);
int test_psd_color_0193(int argc, char **argv);
int test_psd_color_0194(int argc, char **argv);
int test_psd_color_0195(int argc, char **argv);
int test_psd_color_0196(int argc, char **argv);
int test_psd_color_0197(int argc, char **argv);
int test_psd_color_0198(int argc, char **argv);
int test_psd_color_0199(int argc, char **argv);
int test_psd_color_0200(int argc, char **argv);
int test_psd_color_0201(int argc, char **argv);
int test_psd_color_0202(int argc, char **argv);
int test_psd_color_0203(int argc, char **argv);
int test_psd_color_0204(int argc, char **argv);
int test_psd_color_0205(int argc, char **argv);
int test_psd_color_0206(int argc, char **argv);
int test_psd_color_0207(int argc, char **argv);
int test_psd_color_0208(int argc, char **argv);
int test_psd_color_0209(int argc, char **argv);
int test_psd_color_0210(int argc, char **argv);
int test_psd_color_0211(int argc, char **argv);
int test_psd_color_0212(int argc, char **argv);
int test_psd_color_0213(int argc, char **argv);
int test_psd_color_0214(int argc, char **argv);
int test_psd_color_0215(int argc, char **argv);
int test_psd_color_0216(int argc, char **argv);
int test_psd_color_0217(int argc, char **argv);
int test_psd_color_0218(int argc, char **argv);
int test_psd_color_0219(int argc, char **argv);
int test_psd_color_0220(int argc, char **argv);
int test_psd_color_0221(int argc, char **argv);
int test_psd_color_0222(int argc, char **argv);
int test_psd_color_0223(int argc, char **argv);
int test_psd_color_0224(int argc, char **argv);
int test_psd_color_0225(int argc, char **argv);
int test_psd_color_0226(int argc, char **argv);
int test_psd_color_0227(int argc, char **argv);
int test_psd_color_0228(int argc, char **argv);
int test_psd_color_0229(int argc, char **argv);
int test_psd_color_0230(int argc, char **argv);
int test_psd_color_0231(int argc, char **argv);
int test_psd_color_0232(int argc, char **argv);
int test_psd_color_0233(int argc, char **argv);
int test_psd_color_0234(int argc, char **argv);
int test_psd_color_0235(int argc, char **argv);
int test_psd_color_0236(int argc, char **argv);
int test_psd_color_0237(int argc, char **argv);
int test_psd_color_0238(int argc, char **argv);
int test_psd_color_0239(int argc, char **argv);
int test_psd_color_0240(int argc, char **argv);
int test_psd_color_0241(int argc, char **argv);
int test_psd_color_0242(int argc, char **argv);
int test_psd_color_0243(int argc, char **argv);
int test_psd_color_0244(int argc, char **argv);
int test_psd_color_0245(int argc, char **argv);
int test_psd_color_0246(int argc, char **argv);
int test_psd_color_0247(int argc, char **argv);
int test_psd_color_0248(int argc, char **argv);
int test_psd_color_0249(int argc, char **argv);
int test_psd_color_0250(int argc, char **argv);
int test_psd_color_0251(int argc, char **argv);
int test_psd_color_0252(int argc, char **argv);
int test_psd_color_0253(int argc, char **argv);
int test_psd_color_0254(int argc, char **argv);
int test_psd_color_0255(int argc, char **argv);
int test_psd_color_0256(int argc, char **argv);
int test_psd_color_0257(int argc, char **argv);
int test_psd_color_0258(int argc, char **argv);
int test_psd_color_0259(int argc, char **argv);
int test_psd_color_0260(int argc, char **argv);
int test_psd_color_0261(int argc, char **argv);
int test_psd_color_0262(int argc, char **argv);
int test_psd_color_0263(int argc, char **argv);
int test_psd_color_0264(int argc, char **argv);
int test_psd_color_0265(int argc, char **argv);
int test_psd_color_0266(int argc, char **argv);
int test_psd_color_0267(int argc, char **argv);
int test_psd_color_0268(int argc, char **argv);
int test_psd_color_0269(int argc, char **argv);
int test_psd_color_0270(int argc, char **argv);
int test_psd_color_0271(int argc, char **argv);
int test_psd_color_0272(int argc, char **argv);
int test_psd_color_0273(int argc, char **argv);
int test_psd_color_0274(int argc, char **argv);
int test_hdr_2258(int argc, char **argv);
int test_hdr_2259(int argc, char **argv);
int test_hdr_2260(int argc, char **argv);
int test_hdr_2261(int argc, char **argv);
int test_hdr_2262(int argc, char **argv);
int test_hdr_2263(int argc, char **argv);
int test_hdr_2264(int argc, char **argv);


int test_loader_0185_png_source_numeric(int argc, char **argv);
int test_tiff_0142(int argc, char **argv);
int test_cms_0231(int argc, char **argv);
int test_loader_0195_main(int argc, char **argv);
int test_loader_0196_missing(int argc, char **argv);
int test_loader_0197_layers(int argc, char **argv);
int test_loader_0198_channels(int argc, char **argv);
int test_loader_0199_channels(int argc, char **argv);
int test_loader_0200_profile(int argc, char **argv);
int test_loader_0201_profile(int argc, char **argv);
int test_loader_0202_profile(int argc, char **argv);
int test_loader_jp_0054(int argc, char **argv);
int test_loader_jp_0055(int argc, char **argv);
int test_loader_jp_0056(int argc, char **argv);
int test_loader_jp_0057(int argc, char **argv);
int test_loader_jp_0058(int argc, char **argv);
int test_loader_jp_0059(int argc, char **argv);
int test_loader_jp_0060(int argc, char **argv);
int test_loader_jp_0061(int argc, char **argv);
int test_loader_jp_0062(int argc, char **argv);
int test_loader_jp_0063(int argc, char **argv);
int test_loader_jp_0064(int argc, char **argv);
int test_loader_jp_0065(int argc, char **argv);
int test_loader_jp_0066(int argc, char **argv);
int test_loader_jp_0067(int argc, char **argv);
int test_loader_jp_0068(int argc, char **argv);
int test_loader_jp_0069(int argc, char **argv);
int test_loader_jp_0070(int argc, char **argv);
int test_loader_jp_0071(int argc, char **argv);
int test_loader_jp_0072(int argc, char **argv);
int test_loader_jp_0073(int argc, char **argv);
int test_loader_jp_0074(int argc, char **argv);
int test_loader_jp_0075(int argc, char **argv);
int test_loader_jp_0076(int argc, char **argv);
int test_loader_jp_0077(int argc, char **argv);
int test_loader_jp_0078(int argc, char **argv);
int test_loader_jp_0079(int argc, char **argv);
int test_loader_jp_0080(int argc, char **argv);
int test_loader_jp_0081(int argc, char **argv);
int test_loader_jp_0082(int argc, char **argv);
int test_loader_jp_0083(int argc, char **argv);
int test_loader_jp_0084(int argc, char **argv);
int test_loader_jp_0085(int argc, char **argv);
int test_loader_jp_0086(int argc, char **argv);
int test_loader_jp_0087(int argc, char **argv);
int test_loader_jp_0088(int argc, char **argv);
int test_loader_jp_0089(int argc, char **argv);
int test_loader_jp_0090(int argc, char **argv);
int test_loader_jp_0091(int argc, char **argv);
int test_loader_jp_0092(int argc, char **argv);
int test_loader_jp_0093(int argc, char **argv);
int test_loader_jp_0094(int argc, char **argv);
int test_loader_jp_0095(int argc, char **argv);
int test_loader_jp_0096(int argc, char **argv);
int test_loader_jp_0097(int argc, char **argv);
int test_loader_jp_0098(int argc, char **argv);
int test_loader_jp_0099(int argc, char **argv);
int test_loader_jp_0100(int argc, char **argv);
int test_loader_jp_0101(int argc, char **argv);
int test_loader_jp_0102(int argc, char **argv);
int test_loader_jp_0103(int argc, char **argv);
int test_loader_jp_0104(int argc, char **argv);
int test_loader_jp_0105(int argc, char **argv);
int test_loader_jp_0106(int argc, char **argv);
int test_loader_jp_0107(int argc, char **argv);
int test_loader_jp_0108(int argc, char **argv);
int test_loader_jp_0109(int argc, char **argv);
int test_loader_jp_0110(int argc, char **argv);
int test_loader_jp_0111(int argc, char **argv);
int test_loader_jp_0112(int argc, char **argv);
int test_loader_jp_0113(int argc, char **argv);
int test_loader_jp_0114(int argc, char **argv);
int test_loader_jp_0115(int argc, char **argv);
int test_loader_jp_0116(int argc, char **argv);
int test_loader_jp_0117(int argc, char **argv);
int test_loader_jp_0118(int argc, char **argv);
int test_loader_jp_0119(int argc, char **argv);
int test_loader_jp_0120(int argc, char **argv);
int test_loader_jp_0121(int argc, char **argv);
int test_loader_jp_0122(int argc, char **argv);
int test_loader_jp_0123(int argc, char **argv);
int test_loader_jp_0124(int argc, char **argv);
int test_loader_jp_0125(int argc, char **argv);
int test_loader_jp_0126(int argc, char **argv);
int test_loader_jp_0127(int argc, char **argv);
int test_loader_jp_0128(int argc, char **argv);
int test_loader_jp_0129(int argc, char **argv);
int test_loader_jp_0130(int argc, char **argv);
int test_loader_jp_0131(int argc, char **argv);
int test_loader_jp_0132(int argc, char **argv);
int test_loader_jp_0133(int argc, char **argv);
int test_loader_jp_0134(int argc, char **argv);
int test_loader_jp_0135(int argc, char **argv);
int test_loader_jp_0136(int argc, char **argv);
int test_loader_jp_0137(int argc, char **argv);
int test_loader_jp_0138(int argc, char **argv);
int test_loader_jp_0139(int argc, char **argv);
int test_loader_jp_0140(int argc, char **argv);
int test_loader_jp_0141(int argc, char **argv);
int test_loader_jp_0142(int argc, char **argv);
int test_loader_jp_0143(int argc, char **argv);
int test_loader_jp_0144(int argc, char **argv);
int test_loader_jp_0145(int argc, char **argv);
int test_loader_jp_0146(int argc, char **argv);
int test_loader_jp_0147(int argc, char **argv);
int test_loader_jp_0148(int argc, char **argv);
int test_loader_jp_0149(int argc, char **argv);
int test_loader_jp_0150(int argc, char **argv);
int test_loader_jp_0151(int argc, char **argv);
int test_loader_jp_0152(int argc, char **argv);
int test_loader_jp_0153(int argc, char **argv);
int test_loader_jp_0154(int argc, char **argv);
int test_loader_jp_0155(int argc, char **argv);
int test_loader_jp_0163(int argc, char **argv);
int test_loader_jp_0164(int argc, char **argv);
int test_loader_jp_0165(int argc, char **argv);
int test_loader_jp_0166(int argc, char **argv);
int test_loader_jp_0167(int argc, char **argv);
int test_loader_jp_0168(int argc, char **argv);
int test_loader_jp_0169(int argc, char **argv);
int test_loader_jp_0170(int argc, char **argv);
int test_loader_jp_0171(int argc, char **argv);
int test_loader_jp_0172(int argc, char **argv);
int test_loader_jp_0173(int argc, char **argv);
int test_loader_jp_0174(int argc, char **argv);
int test_loader_jp_0175(int argc, char **argv);
int test_loader_jp_0176(int argc, char **argv);
int test_loader_jp_0177(int argc, char **argv);
int test_loader_jp_0178(int argc, char **argv);
int test_loader_jp_0179(int argc, char **argv);
int test_loader_jp_0180(int argc, char **argv);
int test_loader_jp_0181(int argc, char **argv);
int test_loader_jp_0182(int argc, char **argv);
int test_loader_jp_0183(int argc, char **argv);
int test_loader_jp_0184(int argc, char **argv);
int test_loader_jp_0185(int argc, char **argv);
int test_loader_jp_0186(int argc, char **argv);
int test_loader_jp_0187(int argc, char **argv);
int test_loader_jp_0188(int argc, char **argv);
int test_loader_jp_0189(int argc, char **argv);
int test_loader_jp_0190(int argc, char **argv);
int test_loader_jp_0191(int argc, char **argv);
int test_loader_jp_0162(int argc, char **argv);
int test_loader_jp_0161(int argc, char **argv);
int test_loader_jp_0160(int argc, char **argv);
int test_loader_jp_0159(int argc, char **argv);
int test_loader_jp_0158(int argc, char **argv);
int test_loader_jp_0157(int argc, char **argv);
int test_loader_jp_0156(int argc, char **argv);
int test_loader_0203_jpeg_dnl(int argc, char **argv);
int test_loader_0204_jpeg_dnl(int argc, char **argv);
int test_loader_0205_jpeg_dnl(int argc, char **argv);
int test_loader_0206_jpeg_dnl(int argc, char **argv);
int test_loader_0207_jpeg_dnl(int argc, char **argv);
int test_loader_0208_jpeg_dnl(int argc, char **argv);
int test_loader_0209_jpeg_dnl(int argc, char **argv);
int test_loader_0210_jpeg_dnl(int argc, char **argv);
int test_loader_0211_jpeg_dnl(int argc, char **argv);
int test_loader_0212_jpeg_dnl(int argc, char **argv);
int test_loader_0213_jpeg_dnl(int argc, char **argv);
int test_loader_0214_jpeg_dnl(int argc, char **argv);
int test_loader_0215_jpeg_dnl(int argc, char **argv);
int test_loader_0216_jpeg_dnl(int argc, char **argv);
int test_loader_0217_jpeg_dnl(int argc, char **argv);
int test_loader_0218_jpeg_dnl(int argc, char **argv);
int test_loader_0219_jpeg_dnl(int argc, char **argv);
int test_loader_0220_jpeg_dnl(int argc, char **argv);
int test_loader_0221_jpeg_dnl(int argc, char **argv);
int test_loader_0222_jpeg_dnl(int argc, char **argv);
int test_loader_0223_jpeg_dnl(int argc, char **argv);
int test_loader_0224_jpeg_dnl(int argc, char **argv);
int test_loader_0225_jpeg_dnl(int argc, char **argv);
int test_loader_0226_jpeg_dnl(int argc, char **argv);
int test_loader_0227_jpeg_dnl(int argc, char **argv);
int test_loader_0228_tga_origin(int argc, char **argv);
int test_loader_0229_tga_origin(int argc, char **argv);
int test_loader_0230_tga_origin(int argc, char **argv);
int test_loader_0231_tga_origin(int argc, char **argv);
int test_loader_0232_tga_origin(int argc, char **argv);
int test_loader_0233_tga_origin(int argc, char **argv);
int test_loader_0234_tga_origin(int argc, char **argv);
int test_loader_0235_tga_origin(int argc, char **argv);
int test_loader_0236_tga_origin(int argc, char **argv);
int test_loader_0237_tga_origin(int argc, char **argv);
int test_loader_0238_tga_origin(int argc, char **argv);
int test_loader_0239_tga_origin(int argc, char **argv);
int test_loader_0240_tga_origin(int argc, char **argv);
int test_loader_0241_tga_origin(int argc, char **argv);
int test_loader_0172_lab(int argc, char **argv);
int test_loader_0173_lab(int argc, char **argv);
int test_loader_0174_lab16(int argc, char **argv);
int test_cms_0238(int argc, char **argv);
int test_cms_0239(int argc, char **argv);
int test_cms_0240(int argc, char **argv);
int test_cms_0241(int argc, char **argv);
int test_cms_0242(int argc, char **argv);
int test_cms_0243(int argc, char **argv);
int test_cms_0244(int argc, char **argv);
int test_cms_0245(int argc, char **argv);
int test_cms_0246(int argc, char **argv);
int test_cms_0247(int argc, char **argv);
int test_cms_0248(int argc, char **argv);
int test_cms_0249(int argc, char **argv);
int test_cms_0250(int argc, char **argv);
int test_cms_0251(int argc, char **argv);
int test_cms_0252(int argc, char **argv);
int test_cms_0253(int argc, char **argv);
int test_cms_0254(int argc, char **argv);
int test_cms_0255(int argc, char **argv);
int test_cms_0256(int argc, char **argv);
int test_cms_0257(int argc, char **argv);
int test_cms_0258(int argc, char **argv);
int test_cms_0259(int argc, char **argv);
int test_cms_0260(int argc, char **argv);
int test_cms_0261(int argc, char **argv);
int test_cms_0262(int argc, char **argv);
int test_cms_0263(int argc, char **argv);
int test_cms_0264(int argc, char **argv);
int test_cms_0265(int argc, char **argv);
int test_cms_0266(int argc, char **argv);
int test_cms_0267(int argc, char **argv);
int test_cms_0268(int argc, char **argv);
int test_cms_0269(int argc, char **argv);
int test_cms_0270(int argc, char **argv);
int test_cms_0271(int argc, char **argv);
int test_cms_0272(int argc, char **argv);
int test_cms_0273(int argc, char **argv);
int test_cms_0274(int argc, char **argv);
int test_cms_0275(int argc, char **argv);
int test_cms_0276(int argc, char **argv);
int test_cms_0277(int argc, char **argv);
int test_cms_0278(int argc, char **argv);
int test_cms_0279(int argc, char **argv);
int test_cms_0280(int argc, char **argv);
int test_cms_0281(int argc, char **argv);
int test_cms_0282(int argc, char **argv);
int test_cms_0283(int argc, char **argv);
int test_cms_0284(int argc, char **argv);
int test_cms_0285(int argc, char **argv);
int test_cms_0286(int argc, char **argv);
int test_cms_0287(int argc, char **argv);
int test_cms_0288(int argc, char **argv);
int test_cms_0289(int argc, char **argv);
int test_cms_0290(int argc, char **argv);
int test_cms_0291(int argc, char **argv);
int test_cms_0292(int argc, char **argv);
int test_cms_0293(int argc, char **argv);
int test_cms_0294(int argc, char **argv);
int test_cms_0295(int argc, char **argv);
int test_cms_0296(int argc, char **argv);
int test_cms_0297(int argc, char **argv);
int test_cms_0298(int argc, char **argv);
int test_cms_0299(int argc, char **argv);
int test_cms_0300(int argc, char **argv);
int test_cms_0301(int argc, char **argv);
int test_cms_0302(int argc, char **argv);
int test_cms_0303(int argc, char **argv);
int test_cms_0304(int argc, char **argv);
int test_cms_0305(int argc, char **argv);
int test_cms_0306(int argc, char **argv);
int test_cms_0307(int argc, char **argv);
int test_cms_0308(int argc, char **argv);
int test_cms_0309(int argc, char **argv);
int test_cms_0310(int argc, char **argv);
int test_cms_0311(int argc, char **argv);
int test_cms_0312(int argc, char **argv);
int test_cms_0313(int argc, char **argv);
int test_cms_0314(int argc, char **argv);
int test_cms_0315(int argc, char **argv);
int test_cms_0316(int argc, char **argv);
int test_cms_0317(int argc, char **argv);
int test_cms_0318(int argc, char **argv);
int test_cms_0319(int argc, char **argv);
int test_cms_0320(int argc, char **argv);
int test_cms_0321(int argc, char **argv);
int test_cms_0322(int argc, char **argv);
int test_cms_0323(int argc, char **argv);
int test_cms_0324(int argc, char **argv);
int test_cms_0325(int argc, char **argv);
int test_cms_0326(int argc, char **argv);
int test_cms_0327(int argc, char **argv);
int test_cms_0328(int argc, char **argv);
int test_cms_0329(int argc, char **argv);
int test_cms_0330(int argc, char **argv);
int test_cms_0331(int argc, char **argv);
int test_cms_0332(int argc, char **argv);
int test_cms_0333(int argc, char **argv);
int test_cms_0334(int argc, char **argv);
int test_cms_0335(int argc, char **argv);
int test_cms_0336(int argc, char **argv);
int test_cms_0337(int argc, char **argv);
int test_cms_0338(int argc, char **argv);
int test_cms_0339(int argc, char **argv);
int test_cms_0340(int argc, char **argv);
int test_cms_0341(int argc, char **argv);
int test_cms_0342(int argc, char **argv);
int test_cms_0343(int argc, char **argv);
int test_cms_0344_lab_linear(int argc, char **argv);
int test_cms_0345_lab_linear(int argc, char **argv);
int test_cms_0346_lab_linear(int argc, char **argv);
int test_cms_0347_lab_linear(int argc, char **argv);
int test_cms_0348_lab_linear(int argc, char **argv);
int test_cms_0349_lab_linear(int argc, char **argv);
int test_cms_0350_lab_linear(int argc, char **argv);
int test_cms_0351_lab_linear(int argc, char **argv);
int test_cms_0352_mixed(int argc, char **argv);
int test_cms_0353_mixed(int argc, char **argv);
int test_cms_0354_mixed(int argc, char **argv);
int test_cms_0355_mixed(int argc, char **argv);
int test_cms_0356_mixed(int argc, char **argv);
int test_cms_0357_mixed(int argc, char **argv);
int test_cms_0358_mixed(int argc, char **argv);
int test_cms_0359_mixed(int argc, char **argv);
int test_cms_0360_mixed(int argc, char **argv);
int test_cms_0361_mixed(int argc, char **argv);
int test_cms_0362_mixed(int argc, char **argv);
int test_cms_0363_mixed(int argc, char **argv);
int test_cms_0364_mixed(int argc, char **argv);
int test_cms_0365_mixed(int argc, char **argv);
int test_cms_0366_mixed(int argc, char **argv);
int test_cms_0367_mixed(int argc, char **argv);
int test_loader_0184_lab_linear(int argc, char **argv);
int test_loader_0185_lab_linear(int argc, char **argv);
int test_loader_0186_lab_linear(int argc, char **argv);
int test_loader_0187_lab_linear(int argc, char **argv);
int test_loader_0188_lab_linear(int argc, char **argv);
int test_loader_0189_lab_linear(int argc, char **argv);
int test_loader_0190_lab_linear(int argc, char **argv);
int test_loader_0191_lab_linear(int argc, char **argv);
int test_loader_0192_lab_linear(int argc, char **argv);
int test_loader_0193_lab_linear(int argc, char **argv);
int test_loader_0194_lab_linear(int argc, char **argv);
int test_loader_0195_lab_linear(int argc, char **argv);
int test_loader_0196_lab_linear(int argc, char **argv);
int test_loader_0197_lab_linear(int argc, char **argv);
int test_loader_0198_lab_linear(int argc, char **argv);
int test_loader_0199_lab_linear(int argc, char **argv);
int test_loader_0200_logluv(int argc, char **argv);
int test_loader_0201_logluv(int argc, char **argv);
int test_loader_0202_logluv(int argc, char **argv);
int test_loader_0203_logluv(int argc, char **argv);
int test_loader_0204_logluv(int argc, char **argv);
int test_loader_0205_logluv(int argc, char **argv);
int test_loader_0206_logluv(int argc, char **argv);
int test_loader_0207_logluv(int argc, char **argv);
int test_loader_0208_logluv(int argc, char **argv);
int test_loader_0209_logluv(int argc, char **argv);
int test_loader_0210_logluv(int argc, char **argv);
int test_loader_0211_logluv(int argc, char **argv);
int test_loader_0212_logluv(int argc, char **argv);
int test_loader_0213_logluv(int argc, char **argv);
int test_loader_0214_logluv(int argc, char **argv);
int test_loader_0215_logluv(int argc, char **argv);
int test_loader_0216_logluv(int argc, char **argv);
int test_loader_0217_logluv(int argc, char **argv);
int test_loader_0218_logluv(int argc, char **argv);
int test_loader_0219_logluv(int argc, char **argv);
int test_loader_0220_logluv(int argc, char **argv);
int test_loader_0221_logluv(int argc, char **argv);
int test_loader_0222_logluv(int argc, char **argv);
int test_loader_0223_logluv(int argc, char **argv);
int test_loader_0224_logluv(int argc, char **argv);
int test_loader_0225_logluv(int argc, char **argv);
int test_loader_0226_logluv(int argc, char **argv);
int test_loader_0227_tfloat(int argc, char **argv);
int test_loader_0228_tfloat(int argc, char **argv);
int test_loader_0229_tfloat(int argc, char **argv);
int test_loader_0230_tfloat(int argc, char **argv);
int test_loader_0231_tfloat(int argc, char **argv);
int test_loader_0232_tfloat(int argc, char **argv);
int test_loader_0233_tfloat(int argc, char **argv);
int test_loader_0234_tfloat(int argc, char **argv);
int test_loader_0235_tfloat(int argc, char **argv);
int test_loader_0236_tfloat(int argc, char **argv);
int test_loader_0237_tfloat(int argc, char **argv);
int test_loader_0238_tfloat(int argc, char **argv);
int test_loader_0239_tfloat(int argc, char **argv);
int test_loader_0240_tfloat(int argc, char **argv);
int test_loader_0241_tfloat(int argc, char **argv);
int test_loader_0242_tfloat(int argc, char **argv);
int test_loader_0243_tfloat(int argc, char **argv);
int test_loader_0244_tfloat(int argc, char **argv);
int test_loader_0245_tfloat(int argc, char **argv);
int test_loader_0246_tfloat(int argc, char **argv);
int test_loader_0247_tfloat(int argc, char **argv);
int test_loader_0248_tfloat(int argc, char **argv);
int test_loader_0249_tfloat(int argc, char **argv);
int test_loader_0250_mfloat(int argc, char **argv);
int test_loader_0251_mfloat(int argc, char **argv);
int test_loader_0252_mfloat(int argc, char **argv);
int test_loader_0253_mfloat(int argc, char **argv);
int test_loader_0254_mfloat(int argc, char **argv);
int test_loader_0255_mfloat(int argc, char **argv);
int test_loader_0256_mfloat(int argc, char **argv);
int test_loader_0257_mfloat(int argc, char **argv);
int test_loader_0258_mfloat(int argc, char **argv);
int test_loader_0259_mfloat(int argc, char **argv);
int test_loader_0260_mfloat(int argc, char **argv);
int test_loader_0261_mfloat(int argc, char **argv);
int test_loader_0262_mfloat(int argc, char **argv);
int test_loader_0263_mfloat(int argc, char **argv);
int test_loader_0264_mfloat(int argc, char **argv);
int test_loader_0265_mfloat(int argc, char **argv);
int test_loader_0266_mfloat(int argc, char **argv);
int test_loader_0267_mfloat(int argc, char **argv);
int test_loader_0268_mfloat(int argc, char **argv);
int test_loader_0269_mfloat(int argc, char **argv);
int test_loader_0270_mfloat(int argc, char **argv);
int test_loader_0271_mfloat(int argc, char **argv);
int test_loader_0272_mfloat(int argc, char **argv);
int test_loader_0273_mfloat(int argc, char **argv);
int test_loader_0274_mfloat(int argc, char **argv);
int test_loader_0275_mfloat(int argc, char **argv);
int test_loader_0276_mfloat(int argc, char **argv);
int test_loader_0277_mfloat(int argc, char **argv);
int test_loader_0278_mfloat(int argc, char **argv);
int test_loader_0279_mfloat(int argc, char **argv);
int test_loader_0280_gray16(int argc, char **argv);
int test_loader_0281_gray16(int argc, char **argv);
int test_loader_0282_gray16(int argc, char **argv);
int test_loader_0283_gray16(int argc, char **argv);
int test_loader_0284_gray16(int argc, char **argv);
int test_loader_0285_byte(int argc, char **argv);
int test_loader_0286_byte(int argc, char **argv);
int test_loader_0287_byte(int argc, char **argv);
int test_loader_0288_byte(int argc, char **argv);
int test_loader_0289_byte(int argc, char **argv);
int test_loader_0290_byte(int argc, char **argv);
int test_loader_0291_byte(int argc, char **argv);
int test_loader_0292_byte(int argc, char **argv);
int test_loader_0293_byte(int argc, char **argv);
int test_loader_0294_byte(int argc, char **argv);
int test_loader_0295_byte(int argc, char **argv);
int test_loader_0296_byte(int argc, char **argv);
int test_loader_0297_byte(int argc, char **argv);
int test_loader_0298_byte(int argc, char **argv);
int test_loader_0299_byte(int argc, char **argv);
int test_loader_0300_byte(int argc, char **argv);
int test_loader_0301_byte(int argc, char **argv);
int test_loader_0302_byte(int argc, char **argv);
int test_loader_0303_byte(int argc, char **argv);
int test_loader_0304_byte(int argc, char **argv);
int test_loader_0305_alpha(int argc, char **argv);
int test_loader_0306_alpha(int argc, char **argv);
int test_loader_0307_alpha(int argc, char **argv);
int test_loader_0308_alpha(int argc, char **argv);
int test_loader_0309_alpha(int argc, char **argv);
int test_loader_0310_alpha(int argc, char **argv);
int test_loader_0311_alpha(int argc, char **argv);
int test_loader_0312_alpha(int argc, char **argv);
int test_loader_0313_alpha(int argc, char **argv);
int test_loader_0314_alpha(int argc, char **argv);
int test_loader_0315_alpha(int argc, char **argv);
int test_loader_0316_alpha(int argc, char **argv);
int test_loader_0317_alpha(int argc, char **argv);
int test_loader_0318_alpha(int argc, char **argv);
int test_loader_0319_alpha(int argc, char **argv);
int test_loader_0320_alpha(int argc, char **argv);
int test_loader_0321_alpha(int argc, char **argv);
int test_loader_0322_alpha(int argc, char **argv);
int test_loader_0323_alpha(int argc, char **argv);
int test_loader_0324_alpha(int argc, char **argv);
int test_loader_0325_alpha(int argc, char **argv);
int test_loader_0326_alpha(int argc, char **argv);
int test_loader_0327_alpha(int argc, char **argv);
int test_loader_0328_alpha(int argc, char **argv);
int test_loader_0329_alpha(int argc, char **argv);
int test_loader_0330_alpha(int argc, char **argv);
int test_loader_0331_alpha(int argc, char **argv);
int test_loader_0332_alpha(int argc, char **argv);
int test_loader_0333_alpha(int argc, char **argv);
int test_loader_0334_alpha(int argc, char **argv);
int test_loader_0335_alpha16(int argc, char **argv);
int test_loader_0336_alpha16(int argc, char **argv);
int test_loader_0337_alpha16(int argc, char **argv);
int test_loader_0338_alpha16(int argc, char **argv);
int test_loader_0339_alpha16(int argc, char **argv);
int test_loader_0340_alpha16(int argc, char **argv);
int test_loader_0341_alpha16(int argc, char **argv);
int test_loader_0342_alpha16(int argc, char **argv);
int test_loader_0343_alpha16(int argc, char **argv);
int test_loader_0344_alpha16(int argc, char **argv);
int test_loader_0345_alpha16(int argc, char **argv);
int test_loader_0346_alpha16(int argc, char **argv);
int test_loader_0347_alpha16(int argc, char **argv);
int test_loader_0348_alpha16(int argc, char **argv);
int test_loader_0349_alpha16(int argc, char **argv);
int test_loader_0350_alpha16(int argc, char **argv);
int test_loader_0351_alpha16(int argc, char **argv);
int test_loader_0352_alpha16(int argc, char **argv);
int test_loader_0353_alpha16(int argc, char **argv);
int test_loader_0354_alpha16(int argc, char **argv);
int test_loader_0355_alpha16(int argc, char **argv);
int test_loader_0356_alpha16(int argc, char **argv);
int test_loader_0357_alpha16(int argc, char **argv);
int test_loader_0358_alpha16(int argc, char **argv);
int test_loader_0359_alpha16(int argc, char **argv);
int test_loader_0360_alpha16(int argc, char **argv);
int test_loader_0361_alpha16(int argc, char **argv);
int test_loader_0362_alpha16(int argc, char **argv);
int test_loader_0363_alpha16(int argc, char **argv);
int test_loader_0364_alpha16(int argc, char **argv);
int test_loader_0365_alpha16(int argc, char **argv);
int test_loader_0366_legacy(int argc, char **argv);
int test_loader_0367_legacy(int argc, char **argv);
int test_loader_0368_legacy(int argc, char **argv);
int test_loader_0369_legacy(int argc, char **argv);
int test_loader_0370_legacy(int argc, char **argv);
int test_loader_0371_legacy(int argc, char **argv);
int test_loader_0372_legacy(int argc, char **argv);
int test_loader_0373_legacy(int argc, char **argv);
int test_loader_0374_legacy(int argc, char **argv);
int test_loader_0375_legacy(int argc, char **argv);
int test_loader_0376_legacy(int argc, char **argv);
int test_loader_0377_legacy(int argc, char **argv);
int test_loader_0378_legacy(int argc, char **argv);
int test_loader_0379_legacy(int argc, char **argv);
int test_loader_0380_legacy(int argc, char **argv);
int test_loader_0381_legacy(int argc, char **argv);
int test_loader_0382_legacy(int argc, char **argv);
int test_loader_0413_layout(int argc, char **argv);
int test_loader_0414_layout(int argc, char **argv);
int test_loader_0415_layout(int argc, char **argv);
int test_loader_0416_layout(int argc, char **argv);
int test_loader_0417_layout(int argc, char **argv);
int test_loader_0418_layout(int argc, char **argv);
int test_loader_0419_layout(int argc, char **argv);
int test_loader_0420_layout(int argc, char **argv);
int test_loader_0421_layout(int argc, char **argv);
int test_loader_0422_layout(int argc, char **argv);
int test_loader_0423_layout(int argc, char **argv);
int test_loader_0424_layout(int argc, char **argv);
int test_loader_0425_layout(int argc, char **argv);
int test_loader_0426_layout(int argc, char **argv);
int test_loader_0427_layout(int argc, char **argv);
int test_loader_0428_layout(int argc, char **argv);
int test_loader_0429_layout(int argc, char **argv);
int test_loader_0430_layout(int argc, char **argv);
int test_loader_0431_layout(int argc, char **argv);
int test_loader_0432_layout(int argc, char **argv);
int test_loader_0433_layout(int argc, char **argv);
int test_loader_0434_layout(int argc, char **argv);
int test_loader_0435_layout(int argc, char **argv);
int test_loader_0436_layout(int argc, char **argv);
int test_loader_0437_layout(int argc, char **argv);
int test_loader_0438_layout(int argc, char **argv);
int test_loader_0439_layout(int argc, char **argv);
int test_loader_0440_layout(int argc, char **argv);
int test_loader_0441_layout(int argc, char **argv);
int test_loader_0442_layout(int argc, char **argv);
int test_loader_0443_layout(int argc, char **argv);
int test_loader_0444_layout(int argc, char **argv);
int test_loader_0445_layout(int argc, char **argv);
int test_loader_0446_layout(int argc, char **argv);
int test_loader_0447_layout(int argc, char **argv);
int test_loader_0448_layout(int argc, char **argv);
int test_loader_0449_layout(int argc, char **argv);
int test_loader_0450_layout(int argc, char **argv);
int test_loader_0451_layout(int argc, char **argv);
int test_loader_0452_layout(int argc, char **argv);
int test_loader_0453_layout(int argc, char **argv);
int test_loader_0454_layout(int argc, char **argv);
int test_loader_0455_layout(int argc, char **argv);
int test_loader_0456_layout(int argc, char **argv);
int test_loader_0457_layout(int argc, char **argv);
int test_loader_0458_layout(int argc, char **argv);
int test_loader_0459_layout(int argc, char **argv);
int test_loader_0460_layout(int argc, char **argv);
int test_loader_0461_layout(int argc, char **argv);
int test_loader_0462_layout(int argc, char **argv);
int test_loader_0463_layout(int argc, char **argv);
int test_loader_0464_layout(int argc, char **argv);
int test_loader_0465_layout(int argc, char **argv);
int test_loader_0466_layout(int argc, char **argv);
int test_loader_0467_ycbcr(int argc, char **argv);
int test_loader_0468_ycbcr(int argc, char **argv);
int test_loader_0469_ycbcr(int argc, char **argv);
int test_loader_0470_ycbcr(int argc, char **argv);
int test_loader_0471_ycbcr(int argc, char **argv);
int test_loader_0472_ycbcr(int argc, char **argv);
int test_loader_0473_ycbcr(int argc, char **argv);
int test_loader_0474_ycbcr(int argc, char **argv);
int test_loader_0558_device(int argc, char **argv);
int test_loader_0494_device(int argc, char **argv);
int test_loader_0495_device(int argc, char **argv);
int test_loader_0496_device(int argc, char **argv);
int test_loader_0497_device(int argc, char **argv);
int test_loader_0498_device(int argc, char **argv);
int test_loader_0499_device(int argc, char **argv);
int test_loader_0500_device(int argc, char **argv);
int test_loader_0501_device(int argc, char **argv);
int test_loader_0502_device(int argc, char **argv);
int test_loader_0503_device(int argc, char **argv);
int test_loader_0504_device(int argc, char **argv);
int test_loader_0505_device(int argc, char **argv);
int test_loader_0506_device(int argc, char **argv);
int test_loader_0507_device(int argc, char **argv);
int test_loader_0508_device(int argc, char **argv);
int test_loader_0509_device(int argc, char **argv);
int test_loader_0510_device(int argc, char **argv);
int test_loader_0511_device(int argc, char **argv);
int test_loader_0512_device(int argc, char **argv);
int test_loader_0513_device(int argc, char **argv);
int test_loader_0514_device(int argc, char **argv);
int test_loader_0515_device(int argc, char **argv);
int test_loader_0516_device(int argc, char **argv);
int test_loader_0517_device(int argc, char **argv);
int test_loader_0518_device(int argc, char **argv);
int test_loader_0519_device(int argc, char **argv);
int test_loader_0520_device(int argc, char **argv);
int test_loader_0521_device(int argc, char **argv);
int test_loader_0522_device(int argc, char **argv);
int test_loader_0523_device(int argc, char **argv);
int test_loader_0524_device(int argc, char **argv);
int test_loader_0525_device(int argc, char **argv);
int test_loader_0526_device(int argc, char **argv);
int test_loader_0527_device(int argc, char **argv);
int test_loader_0528_device(int argc, char **argv);
int test_loader_0529_device(int argc, char **argv);
int test_loader_0530_device(int argc, char **argv);
int test_loader_0531_device(int argc, char **argv);
int test_loader_0532_device(int argc, char **argv);
int test_loader_0533_device(int argc, char **argv);
int test_loader_0534_device(int argc, char **argv);
int test_loader_0535_device(int argc, char **argv);
int test_loader_0536_device(int argc, char **argv);
int test_loader_0537_device(int argc, char **argv);
int test_loader_0538_device(int argc, char **argv);
int test_loader_0539_device(int argc, char **argv);
int test_loader_0540_device(int argc, char **argv);
int test_loader_0541_device(int argc, char **argv);
int test_loader_0542_device(int argc, char **argv);
int test_loader_0543_device(int argc, char **argv);
int test_loader_0544_device(int argc, char **argv);
int test_loader_0545_device(int argc, char **argv);
int test_loader_0546_device(int argc, char **argv);
int test_loader_0547_device(int argc, char **argv);
int test_loader_0548_device(int argc, char **argv);
int test_loader_0549_device(int argc, char **argv);
int test_loader_0550_device(int argc, char **argv);
int test_loader_0551_device(int argc, char **argv);
int test_loader_0552_device(int argc, char **argv);
int test_loader_0553_device(int argc, char **argv);
int test_loader_0554_device(int argc, char **argv);
int test_loader_0555_device(int argc, char **argv);
int test_loader_0556_device(int argc, char **argv);
int test_loader_0557_device(int argc, char **argv);
int test_loader_0491_lab_layout(int argc, char **argv);
int test_loader_0492_lab_layout(int argc, char **argv);
int test_loader_0493_lab_layout(int argc, char **argv);
int test_loader_0475_lab_layout(int argc, char **argv);
int test_loader_0476_lab_layout(int argc, char **argv);
int test_loader_0477_lab_layout(int argc, char **argv);
int test_loader_0478_lab_layout(int argc, char **argv);
int test_loader_0479_lab_layout(int argc, char **argv);
int test_loader_0480_lab_layout(int argc, char **argv);
int test_loader_0481_lab_layout(int argc, char **argv);
int test_loader_0482_lab_layout(int argc, char **argv);
int test_loader_0483_lab_layout(int argc, char **argv);
int test_loader_0484_lab_layout(int argc, char **argv);
int test_loader_0485_lab_layout(int argc, char **argv);
int test_loader_0486_lab_layout(int argc, char **argv);
int test_loader_0487_lab_layout(int argc, char **argv);
int test_loader_0488_lab_layout(int argc, char **argv);
int test_loader_0489_lab_layout(int argc, char **argv);
int test_loader_0490_lab_layout(int argc, char **argv);
int test_loader_0383_sep16(int argc, char **argv);
int test_loader_0384_sep16(int argc, char **argv);
int test_loader_0385_sep16(int argc, char **argv);
int test_loader_0386_sep16(int argc, char **argv);
int test_loader_0387_sep16(int argc, char **argv);
int test_loader_0388_sep16(int argc, char **argv);
int test_loader_0389_sep16(int argc, char **argv);
int test_loader_0390_sep16(int argc, char **argv);
int test_loader_0391_sep16(int argc, char **argv);
int test_loader_0392_sep16(int argc, char **argv);
int test_loader_0393_sep16(int argc, char **argv);
int test_loader_0394_sep16(int argc, char **argv);
int test_loader_0395_sep16(int argc, char **argv);
int test_loader_0396_sep16(int argc, char **argv);
int test_loader_0397_sep16(int argc, char **argv);
int test_loader_0398_sep16(int argc, char **argv);
int test_loader_0399_sep16(int argc, char **argv);
int test_loader_0400_sep16(int argc, char **argv);
int test_loader_0401_sep16(int argc, char **argv);
int test_loader_0402_sep16(int argc, char **argv);
int test_loader_0403_sep16(int argc, char **argv);
int test_loader_0404_sep16(int argc, char **argv);
int test_loader_0405_sep16(int argc, char **argv);
int test_loader_0406_sep16(int argc, char **argv);
int test_loader_0407_sep16(int argc, char **argv);
int test_loader_0408_sep16(int argc, char **argv);
int test_loader_0409_sep16(int argc, char **argv);
int test_loader_0410_sep16(int argc, char **argv);
int test_loader_0411_sep16(int argc, char **argv);
int test_loader_0412_sep16(int argc, char **argv);
int test_loader_0186_png_byte_target(int argc, char **argv);
int test_loader_0189_byte_storage(int argc, char **argv);
int test_loader_0187_coregraphics_png(int argc, char **argv);
int test_loader_0188_colorsync_linear(int argc, char **argv);
int test_loader_0189_png_palette_samples(int argc, char **argv);
int test_loader_0190_png_palette_failure(int argc, char **argv);
int test_loader_0191_png_palette_paint(int argc, char **argv);
int test_loader_0192_png_metadata_samples(int argc, char **argv);
int test_loader_0193_png_metadata_failures(int argc, char **argv);
int test_loader_0194_png_metadata_ownership(int argc, char **argv);
int test_loader_0195_png_chrm_roundtrip(int argc, char **argv);
int test_loader_0196_png_chrm_boundary(int argc, char **argv);
int test_loader_0197_png_chrm_reject(int argc, char **argv);
int test_loader_0198_png_chrm_samples(int argc, char **argv);
int test_loader_0199_png_chrm_getter_reject(int argc, char **argv);
int test_loader_0200_png_chrm_trace_control(int argc, char **argv);
int test_loader_0201_png_chrm_area(int argc, char **argv);
int test_loader_0202_png_chrm_accept(int argc, char **argv);
int test_loader_0203_png_arith_dispatch(int argc, char **argv);
int test_loader_0204_png_arith_exact(int argc, char **argv);
int test_loader_0205_png_arith_double(int argc, char **argv);
int test_loader_0206_png_arith_neutral(int argc, char **argv);
int test_loader_0207_png_arith_srgb_target(int argc, char **argv);
int test_loader_0208_png_arith_singular(int argc, char **argv);
int test_loader_0209_png_arith_invalid(int argc, char **argv);
int test_loader_0210_png_arith_loader(int argc, char **argv);
int test_loader_0211_png_arith_unavailable(int argc, char **argv);
int test_loader_0212_png_arith_tiny(int argc, char **argv);

int test_loader_0020_loader_librsvg_detect_svg_like(int argc, char **argv);
int test_loader_0021_loader_builtin_indexed_png_reqcolors_fallback(int argc,
                                                                    char **argv
                                                                    );
int test_loader_0022_loader_libpng_indexed_png_reqcolors_fallback(int argc,
                                                                   char **argv);
int test_loader_0023_loader_librsvg_pixelformat(int argc, char **argv);
int test_loader_0025_loader_librsvg_decode_mode(int argc, char **argv);
int test_loader_0026_loader_librsvg_setopt_compat(int argc, char **argv);
int test_loader_0032_loader_librsvg_setopt_ignored_diag(int argc, char **argv);
int test_loader_0033_loader_librsvg_bgcolor_pixelformat(int argc,
                                                        char **argv);

#if defined(SIXEL_ENABLE_GDK_PIXBUF_LOADER_TESTS)
int test_gdk_pixbuf_loader_0001_gdk_pixbuf_loader(int argc, char **argv);
int test_gdk_pixbuf_loader_0002_incremental_load(int argc, char **argv);
int test_gdk_pixbuf_loader_0003_corrupt_data(int argc, char **argv);
int test_gdk_pixbuf_loader_0004_propagate_error(int argc, char **argv);
int test_gdk_pixbuf_loader_0005_context_free(int argc, char **argv);
int test_gdk_pixbuf_loader_0006_loader_module(int argc, char **argv);
int test_gdk_pixbuf_loader_0007_loader_plugin_integration(int argc,
                                                          char **argv);
#endif

int test_png_color_0006(int argc, char **argv);
int test_png_color_0007(int argc, char **argv);
int test_png_color_0008(int argc, char **argv);
int test_png_color_0009(int argc, char **argv);
int test_png_color_0010(int argc, char **argv);
int test_png_color_0011(int argc, char **argv);
int test_png_color_0012(int argc, char **argv);
int test_png_color_0013(int argc, char **argv);
int test_png_color_0017(int argc, char **argv);
int test_png_color_0018(int argc, char **argv);
int test_png_color_0019(int argc, char **argv);
int test_png_color_0020(int argc, char **argv);

int test_png_color_0015(int argc, char **argv);
int test_png_color_0016(int argc, char **argv);
int test_png_color_0021(int argc, char **argv);
int test_png_color_0022(int argc, char **argv);
int test_png_color_0023(int argc, char **argv);
int test_png_color_0024(int argc, char **argv);
int test_png_color_0025(int argc, char **argv);

typedef int (*test_runner_fn)(int argc, char **argv);

typedef struct test_entry {
    char const *name;
    test_runner_fn run;
} test_entry_t;

static test_entry_t const test_entries[] = {
    { "writer/png/0015_p3_resize_coordinates", test_png_color_0015 },
    { "writer/png/0016_p3_k_undither_coordinates", test_png_color_0016 },
    { "writer/png/0021_p3_lso_fs_coordinates", test_png_color_0021 },
    { "writer/png/0022_p3_lso_light_coordinates", test_png_color_0022 },
    { "writer/png/0023_p3_selective_blur_coordinates", test_png_color_0023 },
    { "writer/png/0024_p3_direct_resize_coordinates", test_png_color_0024 },
    { "writer/png/0025_p3_direct_undither_coordinates", test_png_color_0025 },
    { "writer/png/0006_srgb_indexed", test_png_color_0006 },
    { "writer/png/0007_p3_indexed", test_png_color_0007 },
    { "writer/png/0008_p3_direct_alpha", test_png_color_0008 },
    { "writer/png/0009_p3_register_redefinition", test_png_color_0009 },
    { "writer/png/0010_p3_environment", test_png_color_0010 },
    { "writer/png/0011_cli_overrides_environment", test_png_color_0011 },
    { "writer/png/0012_last_interpretation_wins", test_png_color_0012 },
    { "writer/png/0013_long_input_colorspace", test_png_color_0013 },
    { "writer/png/0017_legacy_writer_untagged", test_png_color_0017 },
    { "writer/png/0018_shared_rgb_p3", test_png_color_0018 },
    { "writer/png/0019_raw_decode_unchanged", test_png_color_0019 },
    { "writer/png/0020_icc_numeric_contract", test_png_color_0020 },
    { "filter/0001_filter_clip", test_filter_0001_filter_clip },
    { "filter/0002_filter_sample", test_filter_0002_filter_sample },
    { "filter/0003_filter_resize", test_filter_0003_filter_resize },
    { "filter/0004_filter_colors", test_filter_0004_filter_colors },
    { "filter/0005_filter_lookup", test_filter_0005_filter_lookup },
    { "filter/0006_filter_final_merge", test_filter_0006_filter_final_merge },
    { "filter/0007_filter_fhedt", test_filter_0007_filter_fhedt },
    { "filter/0008_filter_load", test_filter_0008_filter_load },
    { "filter/0009_filter_dither", test_filter_0009_filter_dither },
    { "filter/0010_filter_encode", test_filter_0010_filter_encode },
    { "filter/0011_filter_encode_accumulation_buffer",
      test_filter_0011_filter_encode_accumulation_buffer },
    { "filter/0012_filter_encode_6delta_plane",
      test_filter_0012_filter_encode_6delta_plane },
    { "filter/0013_filter_encode_6delta_keycolor",
      test_filter_0013_filter_encode_6delta_keycolor },
    { "filter/0014_filter_sample_solid",
      test_filter_0014_filter_sample_solid },
    { "perturb/0004_perturb_row_float", test_perturb_0004_row_float },
    { "perturb/0003_perturb_row_8bit", test_perturb_0003_row_8bit },
    { "perturb/0017_perturb_keep_early", test_perturb_0017_keep_early },
    { "perturb/0018_perturb_keep_lookup", test_perturb_0018_keep_lookup },
    { "perturb/0001_perturb_weights", test_perturb_0001_weights },
    { "perturb/0002_perturb_api", test_perturb_0002_api },
    { "dither/clamp/0001_soft_vector", test_clamp_0001 },
    { "dither/clamp/0002_auto_soft", test_clamp_0002 },
    { "dither/clamp/0003_hard_channels", test_clamp_0003 },
    { "dither/clamp/0004_none_extended", test_clamp_0004 },
    { "dither/clamp/0005_source_fixed_point", test_clamp_0005 },
    { "dither/clamp/0006_negative_headroom", test_clamp_0006 },
    { "dither/clamp/0007_zero_headroom", test_clamp_0007 },
    { "dither/clamp/0008_kernel_fs", test_clamp_0008 },
    { "dither/clamp/0009_kernel_atkinson", test_clamp_0009 },
    { "dither/clamp/0010_kernel_jajuni", test_clamp_0010 },
    { "dither/clamp/0011_kernel_stucki", test_clamp_0011 },
    { "dither/clamp/0012_kernel_burkes", test_clamp_0012 },
    { "dither/clamp/0013_kernel_sierra1", test_clamp_0013 },
    { "dither/clamp/0014_kernel_sierra2", test_clamp_0014 },
    { "dither/clamp/0015_kernel_sierra3", test_clamp_0015 },
    { "dither/clamp/0016_kernel_lso2", test_clamp_0016 },
    { "dither/clamp/0017_keep_fs", test_clamp_0017 },
    { "dither/clamp/0018_keep_atkinson", test_clamp_0018 },
    { "dither/clamp/0019_keep_jajuni", test_clamp_0019 },
    { "dither/clamp/0020_keep_stucki", test_clamp_0020 },
    { "dither/clamp/0021_keep_burkes", test_clamp_0021 },
    { "dither/clamp/0022_keep_sierra1", test_clamp_0022 },
    { "dither/clamp/0023_keep_sierra2", test_clamp_0023 },
    { "dither/clamp/0024_keep_sierra3", test_clamp_0024 },
    { "dither/clamp/0025_keep_lso2", test_clamp_0025 },
    { "dither/clamp/0026_option_default", test_clamp_0026 },
    { "dither/clamp/0027_option_short", test_clamp_0027 },
    { "dither/clamp/0028_option_repeat", test_clamp_0028 },
    { "dither/clamp/0029_option_nan", test_clamp_0029 },
    { "dither/clamp/0030_option_minimum", test_clamp_0030 },
    { "dither/clamp/0031_option_maximum", test_clamp_0031 },
    { "dither/clamp/0033_lso2_oklab_lightness", test_clamp_0033 },
    { "dither/clamp/0034_lso2_cielab_lightness", test_clamp_0034 },
    { "dither/clamp/0035_lso2_din99d_lightness", test_clamp_0035 },
    { "dither/clamp/0036_api_invalid", test_clamp_0036 },
    { "dither/clamp/0037_dither_instances", test_clamp_0037 },
    { "dither/clamp/0038_encoder_instances", test_clamp_0038 },
    { "dither/clamp/0039_temporal_history", test_clamp_0039 },
    { "dither/clamp/0040_temporal_reset", test_clamp_0040 },
    { "dither/clamp/0041_fs_vertical_residual", test_clamp_0041 },
    { "dither/clamp/0042_fs_serpentine_residual", test_clamp_0042 },
    { "dither/clamp/0043_parallel_overlap", test_clamp_0043 },
    { "encoder-core/0030_or_float_srgb_center",
      test_or_native_0030 },
    { "encoder-core/0031_or_float_p3_center",
      test_or_native_0031 },
    { "encoder-core/0032_or_float_srgb_indices",
      test_or_native_0032 },
    { "encoder-core/0033_or_float_p3_indices",
      test_or_native_0033 },
    { "encoder-core/0034_or_float_p3_pipeline",
      test_or_native_0034 },
    { "encoder-core/0035_or_float_p3_offset",
      test_or_native_0035 },
    { "encoder-core/0036_or_float_p3_builtin",
      test_or_native_0036 },
    { "encoder-core/0037_or_float_byte_control",
      test_or_native_0037 },
    { "encoder-core/0038_or_float_mask_recovery",
      test_or_native_0038 },
    { "encoder-core/0039_or_float_mono_reject",
      test_or_native_0039 },
    { "encoder-core/0040_or_float_wire_levels",
      test_or_native_0040 },
    { "encoder-core/0041_or_float_wire_pipeline",
      test_or_native_0041 },
    { "codec/macro/0009_srgb_float_define", test_macro_0009 },
    { "codec/macro/0010_p3_float_define", test_macro_0010 },
    { "codec/macro/0011_p3_float_invoke", test_macro_0011 },
    { "codec/macro/0012_p3_float_c1", test_macro_0012 },
    { "codec/macro/0013_p3_float_mask", test_macro_0013 },
    { "codec/macro/0014_float_animation_replay", test_macro_0014 },
    { "codec/macro/0015_byte_define", test_macro_0015 },
    { "codec/macro/0016_float_output_reuse", test_macro_0016 },
    { "codec/macro/0017_highcolor_p3_define", test_macro_0017 },
    { "codec/macro/0018_highcolor_p3_invoke", test_macro_0018 },
    { "codec/macro/0019_highcolor_p3_c1", test_macro_0019 },
    { "codec/macro/0020_highcolor_p3_hls", test_macro_0020 },
    { "codec/macro/0021_highcolor_p3_mask", test_macro_0021 },
    { "codec/macro/0022_highcolor_p3_packed", test_macro_0022 },
    { "codec/macro/0023_highcolor_byte_define", test_macro_0023 },
    { "codec/macro/0024_highcolor_output_reuse", test_macro_0024 },
    { "codec/macro/0025_highcolor_p3_replay", test_macro_0025 },
    { "codec/hls/0014_byte_or_control", test_hls_native_0014 },
    { "codec/hls/0015_serial_or", test_hls_native_0015 },
    { "palette/delta_native/0048_delta_fixed_fs_diffuse",
        test_delta_native_0048 },
    { "palette/delta_native/0049_delta_fixed_fs_skip",
        test_delta_native_0049 },
    { "palette/delta_native/0050_delta_fixed_mask_failure",
        test_delta_native_0050 },
    { "palette/delta_native/0051_delta_image_map_capacity",
        test_delta_native_0051 },
    { "palette/delta_native/0052_delta_byte_workers",
        test_delta_native_0052 },
    { "palette/delta_native/0053_delta_byte_map_workers",
        test_delta_native_0053 },
    { "palette/delta_native/0054_delta_wire_float_palette",
        test_delta_native_0054 },
    { "palette/delta_native/0055_delta_wire_disabled",
        test_delta_native_0055 },
    { "palette/delta_native/0056_delta_wire_generated",
        test_delta_native_0056 },
    { "palette/delta_native/0057_delta_wire_generated_workers",
        test_delta_native_0057 },
    { "palette/delta_native/0058_delta_byte_none_keep",
        test_delta_native_0058 },
    { "palette/delta_native/0059_delta_byte_fs_keep",
        test_delta_native_0059 },
    { "palette/delta_native/0060_delta_byte_atkinson_keep",
        test_delta_native_0060 },
    { "palette/delta_native/0061_delta_byte_lso2_keep",
        test_delta_native_0061 },
    { "palette/delta_native/0062_delta_byte_jajuni_keep",
        test_delta_native_0062 },
    { "palette/delta_native/0063_delta_byte_stucki_keep",
        test_delta_native_0063 },
    { "palette/delta_native/0064_delta_byte_burkes_keep",
        test_delta_native_0064 },
    { "palette/delta_native/0065_delta_byte_sierra1_keep",
        test_delta_native_0065 },
    { "palette/delta_native/0066_delta_byte_sierra2_keep",
        test_delta_native_0066 },
    { "palette/delta_native/0067_delta_byte_sierra3_keep",
        test_delta_native_0067 },
    { "palette/delta_native/0068_delta_byte_a_dither_keep",
        test_delta_native_0068 },
    { "palette/delta_native/0069_delta_byte_x_dither_keep",
        test_delta_native_0069 },
    { "palette/delta_native/0070_delta_byte_bluenoise_dither_keep",
        test_delta_native_0070 },
    { "palette/delta_native/0071_delta_byte_compare",
        test_delta_native_0071 },
    { "palette/delta_native/0072_delta_byte_syntax_reuse",
        test_delta_native_0072 },
    { "palette/delta_native/0073_delta_byte_scope_or",
        test_delta_native_0073 },
    { "palette/delta_native/0074_delta_byte_scope_body",
        test_delta_native_0074 },
    { "palette/delta_native/0075_delta_byte_scope_source",
        test_delta_native_0075 },
    { "palette/delta_native/0076_delta_byte_scope_output",
        test_delta_native_0076 },
    { "palette/delta_native/0077_delta_byte_scope_float",
        test_delta_native_0077 },
    { "palette/delta_native/0078_delta_byte_repeat",
        test_delta_native_0078 },
    { "palette/delta_native/0079_delta_byte_repeat_workers",
        test_delta_native_0079 },
    { "palette/delta_native/0080_delta_byte_grid",
        test_delta_native_0080 },
    { "palette/delta_native/0119_delta_linear_output_inverse",
        test_delta_native_0119 },
    { "palette/delta_native/0120_delta_linear_output_threshold",
        test_delta_native_0120 },
    { "palette/delta_native/0121_delta_linear_output_diffuse",
        test_delta_native_0121 },
    { "palette/delta_native/0122_delta_linear_output_skip",
        test_delta_native_0122 },
    { "palette/delta_native/0123_delta_linear_output_gamma_frames",
        test_delta_native_0123 },
    { "palette/delta_native/0124_delta_linear_output_linear_frames",
        test_delta_native_0124 },
    { "palette/delta_native/0125_delta_linear_output_oklab_frames",
        test_delta_native_0125 },
    { "palette/delta_native/0126_delta_linear_output_cielab_frames",
        test_delta_native_0126 },
    { "palette/delta_native/0127_delta_linear_output_din99d_frames",
        test_delta_native_0127 },
    { "palette/delta_native/0128_delta_linear_output_workers",
        test_delta_native_0128 },
    { "palette/delta_native/0129_delta_linear_output_identity",
        test_delta_native_0129 },
    { "palette/delta_native/0130_delta_smptec_output_inverse",
        test_delta_native_0130 },
    { "palette/delta_native/0131_delta_smptec_output_threshold",
        test_delta_native_0131 },
    { "palette/delta_native/0132_delta_smptec_output_diffuse",
        test_delta_native_0132 },
    { "palette/delta_native/0133_delta_smptec_output_skip",
        test_delta_native_0133 },
    { "palette/delta_native/0134_delta_smptec_output_gamma_frames",
        test_delta_native_0134 },
    { "palette/delta_native/0135_delta_smptec_output_linear_frames",
        test_delta_native_0135 },
    { "palette/delta_native/0136_delta_smptec_output_oklab_frames",
        test_delta_native_0136 },
    { "palette/delta_native/0137_delta_smptec_output_cielab_frames",
        test_delta_native_0137 },
    { "palette/delta_native/0138_delta_smptec_output_din99d_frames",
        test_delta_native_0138 },
    { "palette/delta_native/0139_delta_smptec_output_workers",
        test_delta_native_0139 },
    { "palette/delta_native/0140_delta_smptec_output_identity",
        test_delta_native_0140 },
    { "palette/delta_native/0141_delta_output_inverse_failure",
        test_delta_native_0141 },
    { "palette/delta_native/0142_delta_hls_gamma_gamma_frames",
        test_delta_native_0142 },
    { "palette/delta_native/0143_delta_hls_gamma_linear_frames",
        test_delta_native_0143 },
    { "palette/delta_native/0144_delta_hls_gamma_oklab_frames",
        test_delta_native_0144 },
    { "palette/delta_native/0145_delta_hls_gamma_cielab_frames",
        test_delta_native_0145 },
    { "palette/delta_native/0146_delta_hls_gamma_din99d_frames",
        test_delta_native_0146 },
    { "palette/delta_native/0147_delta_hls_linear_gamma_frames",
        test_delta_native_0147 },
    { "palette/delta_native/0148_delta_hls_linear_linear_frames",
        test_delta_native_0148 },
    { "palette/delta_native/0149_delta_hls_linear_oklab_frames",
        test_delta_native_0149 },
    { "palette/delta_native/0150_delta_hls_linear_cielab_frames",
        test_delta_native_0150 },
    { "palette/delta_native/0151_delta_hls_linear_din99d_frames",
        test_delta_native_0151 },
    { "palette/delta_native/0152_delta_hls_smptec_gamma_frames",
        test_delta_native_0152 },
    { "palette/delta_native/0153_delta_hls_smptec_linear_frames",
        test_delta_native_0153 },
    { "palette/delta_native/0154_delta_hls_smptec_oklab_frames",
        test_delta_native_0154 },
    { "palette/delta_native/0155_delta_hls_smptec_cielab_frames",
        test_delta_native_0155 },
    { "palette/delta_native/0156_delta_hls_smptec_din99d_frames",
        test_delta_native_0156 },
    { "palette/delta_native/0157_delta_hls_p3_gamma_frames",
        test_delta_native_0157 },
    { "palette/delta_native/0158_delta_hls_p3_linear_frames",
        test_delta_native_0158 },
    { "palette/delta_native/0159_delta_hls_p3_oklab_frames",
        test_delta_native_0159 },
    { "palette/delta_native/0160_delta_hls_p3_cielab_frames",
        test_delta_native_0160 },
    { "palette/delta_native/0161_delta_hls_p3_din99d_frames",
        test_delta_native_0161 },
    { "palette/delta_native/0162_delta_hls_workers",
        test_delta_native_0162 },
    { "palette/delta_native/0163_delta_hls_candidate_keep",
        test_delta_native_0163 },
    { "palette/delta_native/0164_delta_hls_candidate_repaint",
        test_delta_native_0164 },
    { "palette/delta_native/0165_delta_hls_cache_isolation",
        test_delta_native_0165 },
    { "palette/delta_native/0166_delta_hls_cache_invalidation",
        test_delta_native_0166 },
    { "palette/delta_native/0167_delta_hls_fixed_mask",
        test_delta_native_0167 },
    { "palette/delta_native/0168_delta_hls_fixed_origin",
        test_delta_native_0168 },
    { "palette/delta_native/0169_delta_hls_fixed_reuse",
        test_delta_native_0169 },
    { "palette/delta_native/0170_delta_hls_residual_diffuse",
        test_delta_native_0170 },
    { "palette/delta_native/0171_delta_hls_residual_skip",
        test_delta_native_0171 },
    { "palette/delta_native/0172_delta_hls_syntax_reuse",
        test_delta_native_0172 },
    { "palette/delta_native/0173_delta_hls_failure_recovery",
        test_delta_native_0173 },
    { "palette/delta_native/0174_delta_hls_field_model",
        test_delta_native_0174 },
    { "palette/delta_native/0175_delta_byte_hls_none",
        test_delta_native_0175 },
    { "palette/delta_native/0176_delta_byte_hls_fs",
        test_delta_native_0176 },
    { "palette/delta_native/0177_delta_byte_hls_atkinson",
        test_delta_native_0177 },
    { "palette/delta_native/0178_delta_byte_hls_jajuni",
        test_delta_native_0178 },
    { "palette/delta_native/0179_delta_byte_hls_stucki",
        test_delta_native_0179 },
    { "palette/delta_native/0180_delta_byte_hls_burkes",
        test_delta_native_0180 },
    { "palette/delta_native/0181_delta_byte_hls_sierra1",
        test_delta_native_0181 },
    { "palette/delta_native/0182_delta_byte_hls_sierra2",
        test_delta_native_0182 },
    { "palette/delta_native/0183_delta_byte_hls_sierra3",
        test_delta_native_0183 },
    { "palette/delta_native/0184_delta_byte_hls_lso2",
        test_delta_native_0184 },
    { "palette/delta_native/0185_delta_byte_hls_a_dither",
        test_delta_native_0185 },
    { "palette/delta_native/0186_delta_byte_hls_x_dither",
        test_delta_native_0186 },
    { "palette/delta_native/0187_delta_byte_hls_bluenoise",
        test_delta_native_0187 },
    { "palette/delta_native/0188_delta_byte_hls_candidate_keep",
        test_delta_native_0188 },
    { "palette/delta_native/0189_delta_byte_hls_candidate_repaint",
        test_delta_native_0189 },
    { "palette/delta_native/0190_delta_byte_hls_frames",
        test_delta_native_0190 },
    { "palette/delta_native/0191_delta_byte_hls_workers",
        test_delta_native_0191 },
    { "palette/delta_native/0192_delta_byte_hls_byte_float",
        test_delta_native_0192 },
    { "palette/delta_native/0193_delta_byte_hls_float_byte",
        test_delta_native_0193 },
    { "palette/delta_native/0194_delta_byte_hls_mask",
        test_delta_native_0194 },
    { "palette/delta_native/0195_delta_byte_hls_origin",
        test_delta_native_0195 },
    { "palette/delta_native/0196_delta_byte_hls_invalidation",
        test_delta_native_0196 },
    { "palette/delta_native/0197_delta_byte_hls_failure",
        test_delta_native_0197 },
    { "palette/delta_native/0198_delta_byte_hls_instances",
        test_delta_native_0198 },
    { "palette/delta_native/0199_delta_byte_hls_snap",
        test_delta_native_0199 },
    { "palette/delta_native/0200_delta_byte_hls_float_palette",
        test_delta_native_0200 },
    { "palette/delta_native/0201_delta_mono_then_delta",
        test_delta_native_0201 },
    { "palette/delta_native/0202_delta_delta_then_mono",
        test_delta_native_0202 },
    { "palette/delta_native/0203_delta_mono_environment",
        test_delta_native_0203 },
    { "palette/delta_native/0204_delta_mono_full_override",
        test_delta_native_0204 },
    { "palette/delta_native/0205_delta_mono_output_reject",
        test_delta_native_0205 },
    { "palette/delta_native/0206_delta_opaque_bw_frames",
        test_delta_native_0206 },
    { "palette/delta_native/0207_delta_or_then_delta",
        test_delta_native_0207 },
    { "palette/delta_native/0208_delta_delta_then_or",
        test_delta_native_0208 },
    { "palette/delta_native/0209_delta_or_environment",
        test_delta_native_0209 },
    { "palette/delta_native/0210_delta_or_full_override",
        test_delta_native_0210 },
    { "palette/delta_native/0211_delta_or_byte_recovery",
        test_delta_native_0211 },
    { "palette/delta_native/0212_delta_or_float_recovery",
        test_delta_native_0212 },
    { "palette/delta_native/0213_delta_or_p3_recovery",
        test_delta_native_0213 },
    { "palette/delta_native/0214_delta_drcs_then_delta",
        test_delta_native_0214 },
    { "palette/delta_native/0215_delta_delta_then_drcs",
        test_delta_native_0215 },
    { "palette/delta_native/0216_delta_drcs_environment",
        test_delta_native_0216 },
    { "palette/delta_native/0217_delta_drcs_full_override",
        test_delta_native_0217 },
    { "palette/delta_native/0218_delta_drcs_tile_untouched",
        test_delta_native_0218 },
    { "palette/delta_native/0219_delta_drcs_byte_recovery",
        test_delta_native_0219 },
    { "palette/delta_native/0220_delta_drcs_float_recovery",
        test_delta_native_0220 },
    { "palette/delta_native/0221_delta_drcs_p3_recovery",
        test_delta_native_0221 },
    { "palette/delta_native/0222_delta_macro_use_then_delta",
        test_delta_native_0222 },
    { "palette/delta_native/0223_delta_delta_then_macro_use",
        test_delta_native_0223 },
    { "palette/delta_native/0224_delta_macro_number_then_delta",
        test_delta_native_0224 },
    { "palette/delta_native/0225_delta_delta_then_macro_number",
        test_delta_native_0225 },
    { "palette/delta_native/0226_delta_macro_use_environment",
        test_delta_native_0226 },
    { "palette/delta_native/0227_delta_macro_number_environment",
        test_delta_native_0227 },
    { "palette/delta_native/0228_delta_macro_use_full_override",
        test_delta_native_0228 },
    { "palette/delta_native/0229_delta_macro_number_full_override",
        test_delta_native_0229 },
    { "palette/delta_native/0230_delta_macro_byte_recovery",
        test_delta_native_0230 },
    { "palette/delta_native/0231_delta_macro_float_recovery",
        test_delta_native_0231 },
    { "palette/delta_native/0232_delta_macro_p3_recovery",
        test_delta_native_0232 },
    { "palette/delta_native/0233_delta_macro_combined_selection",
        test_delta_native_0233 },
    { "palette/delta_native/0234_delta_refine_byte",
        test_delta_native_0234 },
    { "palette/delta_native/0235_delta_refine_float",
        test_delta_native_0235 },
    { "palette/delta_native/0236_delta_refine_gpu",
        test_delta_native_0236 },
    { "palette/delta_native/0237_delta_refine_exact",
        test_delta_native_0237 },
    { "palette/delta_native/0238_delta_refine_partial",
        test_delta_native_0238 },
    { "palette/delta_native/0239_delta_refine_source_mask",
        test_delta_native_0239 },
    { "palette/delta_native/0101_delta_p3_gamma_transform",
        test_delta_native_0101 },
    { "palette/delta_native/0102_delta_p3_linear_transform",
        test_delta_native_0102 },
    { "palette/delta_native/0103_delta_p3_oklab_transform",
        test_delta_native_0103 },
    { "palette/delta_native/0104_delta_p3_cielab_transform",
        test_delta_native_0104 },
    { "palette/delta_native/0105_delta_p3_din99d_transform",
        test_delta_native_0105 },
    { "palette/delta_native/0106_delta_p3_threshold",
        test_delta_native_0106 },
    { "palette/delta_native/0107_delta_p3_diffuse",
        test_delta_native_0107 },
    { "palette/delta_native/0108_delta_p3_skip",
        test_delta_native_0108 },
    { "palette/delta_native/0109_delta_p3_cache_lifetime",
        test_delta_native_0109 },
    { "palette/delta_native/0110_delta_p3_cache_failure",
        test_delta_native_0110 },
    { "palette/delta_native/0111_delta_p3_gamma_frames",
        test_delta_native_0111 },
    { "palette/delta_native/0112_delta_p3_linear_frames",
        test_delta_native_0112 },
    { "palette/delta_native/0113_delta_p3_oklab_frames",
        test_delta_native_0113 },
    { "palette/delta_native/0114_delta_p3_cielab_frames",
        test_delta_native_0114 },
    { "palette/delta_native/0115_delta_p3_din99d_frames",
        test_delta_native_0115 },
    { "palette/delta_native/0116_delta_p3_workers",
        test_delta_native_0116 },
    { "palette/delta_native/0117_delta_p3_definition_switch",
        test_delta_native_0117 },
    { "palette/delta_native/0118_delta_p3_seed_definition",
        test_delta_native_0118 },
    { "palette/delta_native/0081_delta_linear_frames",
        test_delta_native_0081 },
    { "palette/delta_native/0082_delta_oklab_frames",
        test_delta_native_0082 },
    { "palette/delta_native/0083_delta_cielab_frames",
        test_delta_native_0083 },
    { "palette/delta_native/0084_delta_din99d_frames",
        test_delta_native_0084 },
    { "palette/delta_native/0085_delta_linear_diffuse",
        test_delta_native_0085 },
    { "palette/delta_native/0086_delta_linear_skip",
        test_delta_native_0086 },
    { "palette/delta_native/0087_delta_linear_compare",
        test_delta_native_0087 },
    { "palette/delta_native/0088_delta_oklab_compare",
        test_delta_native_0088 },
    { "palette/delta_native/0089_delta_cielab_compare",
        test_delta_native_0089 },
    { "palette/delta_native/0090_delta_din99d_compare",
        test_delta_native_0090 },
    { "palette/delta_native/0091_delta_linear_extended",
        test_delta_native_0091 },
    { "palette/delta_native/0092_delta_workspace_fixed",
        test_delta_native_0092 },
    { "palette/delta_native/0093_delta_workspace_workers",
        test_delta_native_0093 },
    { "palette/delta_native/0094_delta_workspace_mask",
        test_delta_native_0094 },
    { "palette/delta_native/0095_delta_workspace_origin",
        test_delta_native_0095 },
    { "palette/delta_native/0096_delta_workspace_bands",
        test_delta_native_0096 },
    { "palette/delta_native/0097_delta_workspace_domain",
        test_delta_native_0097 },
    { "palette/delta_native/0098_delta_workspace_legacy",
        test_delta_native_0098 },
    { "palette/delta_native/0099_delta_virtual_guard",
        test_delta_native_0099 },
    { "palette/delta_native/0100_delta_virtual_fs",
        test_delta_native_0100 },
    { "palette/delta_native/0033_delta_builtin_slots",
        test_delta_native_0033 },
    { "palette/delta_native/0034_delta_map_slots",
        test_delta_native_0034 },
    { "palette/delta_native/0035_delta_map_duplicates",
        test_delta_native_0035 },
    { "palette/delta_native/0036_delta_builtin_mask",
        test_delta_native_0036 },
    { "palette/delta_native/0037_delta_map_mask",
        test_delta_native_0037 },
    { "palette/delta_native/0038_delta_builtin_origin",
        test_delta_native_0038 },
    { "palette/delta_native/0039_delta_map_origin",
        test_delta_native_0039 },
    { "palette/delta_native/0040_delta_builtin_workers",
        test_delta_native_0040 },
    { "palette/delta_native/0041_delta_map_workers",
        test_delta_native_0041 },
    { "palette/delta_native/0042_delta_builtin_animation",
        test_delta_native_0042 },
    { "palette/delta_native/0043_delta_map_animation",
        test_delta_native_0043 },
    { "palette/delta_native/0044_delta_builtin_byte",
        test_delta_native_0044 },
    { "palette/delta_native/0045_delta_map_byte",
        test_delta_native_0045 },
    { "palette/delta_native/0046_delta_builtin_reuse",
        test_delta_native_0046 },
    { "palette/delta_native/0047_delta_map_reuse",
        test_delta_native_0047 },
    { "palette/delta_native/0018_delta_a_source",
        test_delta_native_0018 },
    { "palette/delta_native/0019_delta_x_source",
        test_delta_native_0019 },
    { "palette/delta_native/0020_delta_blue_source",
        test_delta_native_0020 },
    { "palette/delta_native/0021_delta_a_frames",
        test_delta_native_0021 },
    { "palette/delta_native/0022_delta_x_frames",
        test_delta_native_0022 },
    { "palette/delta_native/0023_delta_blue_frames",
        test_delta_native_0023 },
    { "palette/delta_native/0024_delta_a_workers",
        test_delta_native_0024 },
    { "palette/delta_native/0025_delta_x_workers",
        test_delta_native_0025 },
    { "palette/delta_native/0026_delta_blue_workers",
        test_delta_native_0026 },
    { "palette/delta_native/0027_delta_a_byte",
        test_delta_native_0027 },
    { "palette/delta_native/0028_delta_x_byte",
        test_delta_native_0028 },
    { "palette/delta_native/0029_delta_blue_byte",
        test_delta_native_0029 },
    { "palette/delta_native/0030_delta_a_band",
        test_delta_native_0030 },
    { "palette/delta_native/0031_delta_x_band",
        test_delta_native_0031 },
    { "palette/delta_native/0032_delta_blue_band",
        test_delta_native_0032 },
    { "palette/delta_native/0015_delta_signed_threshold",
        test_delta_native_0015 },
    { "palette/delta_native/0016_delta_p3_or_recovery",
        test_delta_native_0016 },
    { "palette/delta_native/0017_delta_spatial_dispatch",
        test_delta_native_0017 },
    { "palette/delta_native/0001_delta_float_centroid",
        test_delta_native_0001 },
    { "palette/delta_native/0002_delta_byte_control",
        test_delta_native_0002 },
    { "palette/delta_native/0003_delta_plane_origin",
        test_delta_native_0003 },
    { "palette/delta_native/0004_delta_source_mask",
        test_delta_native_0004 },
    { "palette/delta_native/0005_delta_parallel_rows",
        test_delta_native_0005 },
    { "palette/delta_native/0006_delta_residual_fs",
        test_delta_native_0006 },
    { "palette/delta_native/0007_delta_residual_atkinson",
        test_delta_native_0007 },
    { "palette/delta_native/0008_delta_residual_jajuni",
        test_delta_native_0008 },
    { "palette/delta_native/0009_delta_residual_stucki",
        test_delta_native_0009 },
    { "palette/delta_native/0010_delta_residual_burkes",
        test_delta_native_0010 },
    { "palette/delta_native/0011_delta_residual_sierra1",
        test_delta_native_0011 },
    { "palette/delta_native/0012_delta_residual_sierra2",
        test_delta_native_0012 },
    { "palette/delta_native/0013_delta_residual_sierra3",
        test_delta_native_0013 },
    { "palette/delta_native/0014_delta_residual_lso2",
        test_delta_native_0014 },
    { "palette/export_capture/0001_native_capture_memory",
        test_export_capture_0001 },
    { "palette/export_capture/0002_byte_capture_memory",
        test_export_capture_0002 },
    { "palette/export_capture/0003_native_writer_memory",
        test_export_capture_0003 },
    { "palette/export_capture/0004_byte_writer_memory",
        test_export_capture_0004 },
    { "palette/export_capture/0005_reuse_count",
        test_export_capture_0005 },
    { "palette/export_capture/0006_encoder_isolation",
        test_export_capture_0006 },
    { "codec/png_snapshot/0010_delta_stdout_then_delta",
        test_png_snapshot_0010 },
    { "codec/png_snapshot/0011_delta_delta_then_stdout",
        test_png_snapshot_0011 },
    { "codec/png_snapshot/0012_delta_prefix_then_delta",
        test_png_snapshot_0012 },
    { "codec/png_snapshot/0013_delta_delta_then_prefix",
        test_png_snapshot_0013 },
    { "codec/png_snapshot/0014_delta_suffix_then_delta",
        test_png_snapshot_0014 },
    { "codec/png_snapshot/0015_delta_delta_then_suffix",
        test_png_snapshot_0015 },
    { "codec/png_snapshot/0016_delta_environment",
        test_png_snapshot_0016 },
    { "codec/png_snapshot/0017_delta_full_override",
        test_png_snapshot_0017 },
    { "codec/png_snapshot/0018_delta_sixel_switch",
        test_png_snapshot_0018 },
    { "codec/png_snapshot/0019_delta_byte_recovery",
        test_png_snapshot_0019 },
    { "codec/png_snapshot/0020_delta_float_recovery",
        test_png_snapshot_0020 },
    { "codec/png_snapshot/0021_delta_p3_recovery",
        test_png_snapshot_0021 },
    { "codec/png_snapshot/0022_delta_file_guard",
        test_png_snapshot_0022 },
    { "codec/png_snapshot/0023_delta_stdout_guard",
        test_png_snapshot_0023 },
    { "codec/highcolor/0001_byte_identity", test_highcolor_0001 },
    { "codec/highcolor/0002_byte_linear", test_highcolor_0002 },
    { "codec/highcolor/0003_float_subbyte", test_highcolor_0003 },
    { "codec/highcolor/0004_linear_gamma", test_highcolor_0004 },
    { "codec/highcolor/0005_extended_smptec", test_highcolor_0005 },
    { "codec/highcolor/0006_float_hls", test_highcolor_0006 },
    { "codec/highcolor/0007_nan_recovery", test_highcolor_0007 },
    { "codec/highcolor/0008_infinity_recovery", test_highcolor_0008 },
    { "codec/highcolor/0009_rgba_linear", test_highcolor_0009 },
    { "codec/highcolor/0010_frame_float", test_highcolor_0010 },
    { "codec/highcolor/0011_frame_extended", test_highcolor_0011 },
    { "codec/highcolor/0012_multipass_float", test_highcolor_0012 },
    { "codec/highcolor/0013_lowlevel_diffusion", test_highcolor_0013 },
    { "codec/highcolor/0014_context_reuse", test_highcolor_0014 },
    { "codec/highcolor/0015_p3_gamma", test_highcolor_0015 },
    { "codec/highcolor/0016_p3_linear", test_highcolor_0016 },
    { "codec/highcolor/0017_p3_oklab", test_highcolor_0017 },
    { "codec/highcolor/0018_p3_cielab", test_highcolor_0018 },
    { "codec/highcolor/0019_p3_din99d", test_highcolor_0019 },
    { "codec/highcolor/0020_p3_byte", test_highcolor_0020 },
    { "codec/highcolor/0021_p3_subbyte", test_highcolor_0021 },
    { "codec/highcolor/0022_p3_hls", test_highcolor_0022 },
    { "codec/highcolor/0023_p3_multipass", test_highcolor_0023 },
    { "codec/highcolor/0024_p3_workers", test_highcolor_0024 },
    { "codec/highcolor/0025_p3_reuse", test_highcolor_0025 },
    { "codec/highcolor/0026_p3_short_mask_recovery", test_highcolor_0026 },
    { "codec/highcolor/0027_p3_delta_recovery", test_highcolor_0027 },
    { "codec/highcolor/0028_p3_or_recovery", test_highcolor_0028 },
    { "codec/highcolor/0029_p3_drcs_offset_recovery", test_highcolor_0029 },
    { "codec/highcolor/0030_p3_macro_png_recovery", test_highcolor_0030 },
    { "codec/highcolor/0031_p3_png_sixel_reuse", test_highcolor_0031 },
    { "codec/highcolor/0032_p3_snap_recovery", test_highcolor_0032 },
    { "codec/highcolor/0033_p3_nan_recovery", test_highcolor_0033 },
    { "codec/highcolor/0034_p3_alpha_space_recovery", test_highcolor_0034 },
    { "codec/highcolor/0035_final_full", test_highcolor_0035 },
    { "codec/highcolor/0036_final_one", test_highcolor_0036 },
    { "codec/highcolor/0037_final_two", test_highcolor_0037 },
    { "codec/highcolor/0038_final_three", test_highcolor_0038 },
    { "codec/highcolor/0039_final_four", test_highcolor_0039 },
    { "codec/highcolor/0040_final_five", test_highcolor_0040 },
    { "codec/highcolor/0041_single_row", test_highcolor_0041 },
    { "codec/highcolor/0042_key_revisit", test_highcolor_0042 },
    { "codec/highcolor/0043_final_float", test_highcolor_0043 },
    { "codec/highcolor/0044_final_offset", test_highcolor_0044 },
    { "codec/highcolor/0045_short_offset", test_highcolor_0045 },
    { "codec/highcolor/0046_final_workers", test_highcolor_0046 },
    { "codec/highcolor/0047_slots_255", test_highcolor_0047 },
    { "codec/highcolor/0048_slots_256", test_highcolor_0048 },
    { "codec/highcolor/0049_single_offset", test_highcolor_0049 },
    { "codec/highcolor/0050_mask_byte", test_highcolor_0050 },
    { "codec/highcolor/0051_mask_float", test_highcolor_0051 },
    { "codec/highcolor/0052_mask_p3", test_highcolor_0052 },
    { "codec/highcolor/0053_mask_keep", test_highcolor_0053 },
    { "codec/highcolor/0054_mask_clear", test_highcolor_0054 },
    { "codec/highcolor/0055_mask_composite", test_highcolor_0055 },
    { "codec/highcolor/0056_mask_multipass", test_highcolor_0056 },
    { "codec/highcolor/0057_mask_p3_multipass", test_highcolor_0057 },
    { "codec/highcolor/0058_mask_partial", test_highcolor_0058 },
    { "codec/highcolor/0059_mask_offset", test_highcolor_0059 },
    { "codec/highcolor/0060_mask_workers", test_highcolor_0060 },
    { "codec/highcolor/0061_mask_hidden_key", test_highcolor_0061 },
    { "codec/highcolor/0062_mask_all", test_highcolor_0062 },
    { "codec/highcolor/0063_mask_zero", test_highcolor_0063 },
    { "codec/highcolor/0064_mask_reuse", test_highcolor_0064 },
    { "codec/highcolor/0065_mask_core_recovery", test_highcolor_0065 },
    { "codec/highcolor/0066_packed_rgba", test_highcolor_0066 },
    { "codec/highcolor/0067_packed_argb_p3", test_highcolor_0067 },
    { "codec/highcolor/0068_packed_bgra_linear", test_highcolor_0068 },
    { "codec/highcolor/0069_packed_abgr_oklab", test_highcolor_0069 },
    { "codec/highcolor/0070_packed_ga_cielab", test_highcolor_0070 },
    { "codec/highcolor/0071_packed_ag_din99d", test_highcolor_0071 },
    { "codec/highcolor/0072_packed_binary_byte", test_highcolor_0072 },
    { "codec/highcolor/0073_packed_binary_float", test_highcolor_0073 },
    { "codec/highcolor/0074_packed_alpha_disabled", test_highcolor_0074 },
    { "codec/highcolor/0075_packed_mask_union", test_highcolor_0075 },
    { "codec/highcolor/0076_packed_background", test_highcolor_0076 },
    { "codec/highcolor/0077_packed_crop", test_highcolor_0077 },
    { "codec/highcolor/0078_packed_resize", test_highcolor_0078 },
    { "codec/highcolor/0079_packed_reuse", test_highcolor_0079 },
    { "codec/highcolor/0080_packed_pixel_retry", test_highcolor_0080 },
    { "codec/highcolor/0081_packed_mask_retry", test_highcolor_0081 },
    { "codec/highcolor/0082_packed_handoff_plan", test_highcolor_0082 },
    { "codec/highcolor/0083_packed_png_p3", test_highcolor_0083 },
    { "codec/png_snapshot/0024_highcolor_srgb_byte", test_png_snapshot_0024 },
    { "codec/png_snapshot/0025_highcolor_srgb_float", test_png_snapshot_0025 },
    { "codec/png_snapshot/0026_highcolor_p3_rgb", test_png_snapshot_0026 },
    { "codec/png_snapshot/0027_highcolor_p3_hls", test_png_snapshot_0027 },
    { "codec/png_snapshot/0028_highcolor_p3_mask", test_png_snapshot_0028 },
    { "codec/png_snapshot/0029_highcolor_reuse", test_png_snapshot_0029 },
    { "codec/png_snapshot/0030_highcolor_suffix", test_png_snapshot_0030 },
    { "codec/png_snapshot/0031_highcolor_stdout", test_png_snapshot_0031 },
    { "codec/png_snapshot/0001_srgb_frame_precision", test_png_snapshot_0001 },
    { "codec/png_snapshot/0002_p3_frame_precision", test_png_snapshot_0002 },
    { "codec/png_snapshot/0003_byte_frame_control", test_png_snapshot_0003 },
    { "codec/png_snapshot/0004_frame_output_reuse", test_png_snapshot_0004 },
    { "codec/png_snapshot/0005_srgb_rgb_pixels", test_png_snapshot_0005 },
    { "codec/png_snapshot/0006_p3_rgb_pixels", test_png_snapshot_0006 },
    { "codec/png_snapshot/0007_srgb_hls_pixels", test_png_snapshot_0007 },
    { "codec/png_snapshot/0008_p3_hls_pixels", test_png_snapshot_0008 },
    { "codec/png_snapshot/0009_transport_rejection",
      test_png_snapshot_0009 },
    { "codec/hls/0001_srgb_centroid", test_hls_native_0001 },
    { "codec/hls/0002_p3_centroid", test_hls_native_0002 },
    { "codec/hls/0003_srgb_rounding", test_hls_native_0003 },
    { "codec/hls/0004_p3_rounding", test_hls_native_0004 },
    { "codec/hls/0005_hue_and_gamut", test_hls_native_0005 },
    { "codec/hls/0006_builtin_float", test_hls_native_0006 },
    { "codec/hls/0007_byte_control", test_hls_native_0007 },
    { "codec/hls/0008_p3_mask", test_hls_native_0008 },
    { "codec/hls/0009_parallel_body", test_hls_native_0009 },
    { "codec/hls/0010_parallel_or", test_hls_native_0010 },
    { "codec/hls/0011_output_reuse", test_hls_native_0011 },
    { "codec/hls/0012_cache_isolation", test_hls_native_0012 },
    { "codec/hls/0013_cache_invalidation", test_hls_native_0013 },
    { "codec/drcs/0001_srgb_float_center", test_drcs_native_0001 },
    { "codec/drcs/0002_p3_float_center", test_drcs_native_0002 },
    { "codec/drcs/0003_p3_c1_body", test_drcs_native_0003 },
    { "codec/drcs/0004_p3_v3_grid", test_drcs_native_0004 },
    { "codec/drcs/0005_p3_builtin", test_drcs_native_0005 },
    { "codec/drcs/0006_byte_control", test_drcs_native_0006 },
    { "codec/drcs/0007_mask_recovery", test_drcs_native_0007 },
    { "codec/drcs/0008_mono_reject", test_drcs_native_0008 },
    { "codec/drcs/0009_or_reject", test_drcs_native_0009 },
    { "codec/drcs/0010_offset_reject", test_drcs_native_0010 },
    { "codec/drcs/0011_macro_reject", test_drcs_native_0011 },
    { "codec/drcs/0012_output_reuse", test_drcs_native_0012 },
    { "codec/drcs/0013_p3_pipeline", test_drcs_native_0013 },
    { "codec/drcs/0014_highcolor_p3_final_band", test_drcs_highcolor_0014 },
    { "codec/drcs/0015_highcolor_p3_c1", test_drcs_highcolor_0015 },
    { "codec/drcs/0016_highcolor_p3_v3_grid", test_drcs_highcolor_0016 },
    { "codec/drcs/0017_highcolor_p3_hls", test_drcs_highcolor_0017 },
    { "codec/drcs/0018_highcolor_byte_control", test_drcs_highcolor_0018 },
    { "codec/drcs/0019_highcolor_output_reuse", test_drcs_highcolor_0019 },
    { "codec/drcs/0020_highcolor_mask_recovery", test_drcs_highcolor_0020 },
    { "codec/drcs/0021_highcolor_packed_recovery", test_drcs_highcolor_0021 },
    { "codec/drcs/0022_highcolor_macro_recovery", test_drcs_highcolor_0022 },
    { "codec/drcs/0023_highcolor_png_recovery", test_drcs_highcolor_0023 },
    { "codec/drcs/0024_highcolor_writer_restore", test_drcs_highcolor_0024 },
    { "palette/map_cluster/0001_map_linear_mean",
      test_map_cluster_0001 },
    { "palette/map_cluster/0002_map_oklab_mean",
      test_map_cluster_0002 },
    { "palette/map_cluster/0003_map_cielab_negative",
      test_map_cluster_0003 },
    { "palette/map_cluster/0004_map_din99d_mean",
      test_map_cluster_0004 },
    { "palette/map_cluster/0005_map_heckbert_mean",
      test_map_cluster_0005 },
    { "palette/map_cluster/0006_map_center_endpoints",
      test_map_cluster_0006 },
    { "palette/map_cluster/0007_map_medoids_endpoints",
      test_map_cluster_0007 },
    { "palette/map_cluster/0008_map_auto_mean",
      test_map_cluster_0008 },
    { "palette/map_cluster/0009_map_byte_request",
      test_map_cluster_0009 },
    { "palette/map_cluster/0010_map_gamma_byte",
      test_map_cluster_0010 },
    { "palette/map_cluster/0011_map_structured_byte",
      test_map_cluster_0011 },
    { "palette/map_cluster/0012_map_clustering_reuse",
      test_map_cluster_0012 },
    { "palette/map_cluster/0013_map_animation_cache",
      test_map_cluster_0013 },
    { "palette/cluster_precision/0001_linear_mean",
      test_cluster_precision_0001 },
    { "palette/cluster_precision/0002_oklab_mean",
      test_cluster_precision_0002 },
    { "palette/cluster_precision/0003_cielab_negative",
      test_cluster_precision_0003 },
    { "palette/cluster_precision/0004_din99d_mean",
      test_cluster_precision_0004 },
    { "palette/cluster_precision/0005_heckbert_mean",
      test_cluster_precision_0005 },
    { "palette/cluster_precision/0006_center_endpoints",
      test_cluster_precision_0006 },
    { "palette/cluster_precision/0007_medoids_endpoints",
      test_cluster_precision_0007 },
    { "palette/cluster_precision/0008_auto_mean",
      test_cluster_precision_0008 },
    { "palette/cluster_precision/0009_gamma_byte_control",
      test_cluster_precision_0009 },
    { "palette/cluster_precision/0010_builtin_byte_control",
      test_cluster_precision_0010 },
    { "palette/cluster_precision/0011_byte_request",
      test_cluster_precision_0011 },
    { "palette/cluster_precision/0012_workspace_reuse",
      test_cluster_precision_0012 },
    { "palette/cluster_precision/0013_gamma_soft_control",
      test_cluster_precision_0013 },
    { "palette/cluster_precision/0014_linear_output",
      test_cluster_precision_0014 },

    { "dither/clamp/0047_snapshot_allocation", test_clamp_0047 },
    { "dither/clamp/0032_kernel_interframe", test_clamp_0032 },
    { "filter/0015_filter_dither_6delta_lso2",
      test_filter_0015_filter_dither_6delta_lso2 },
    { "filter/0016_filter_dither_6delta_jajuni",
      test_filter_0016_filter_dither_6delta_jajuni },
    { "filter/0017_filter_dither_6delta_stucki",
      test_filter_0017_filter_dither_6delta_stucki },
    { "filter/0018_filter_dither_6delta_burkes",
      test_filter_0018_filter_dither_6delta_burkes },
    { "filter/0019_filter_dither_6delta_sierra1",
      test_filter_0019_filter_dither_6delta_sierra1 },
    { "filter/0020_filter_dither_6delta_sierra2",
      test_filter_0020_filter_dither_6delta_sierra2 },
    { "filter/0021_filter_dither_6delta_sierra3",
      test_filter_0021_filter_dither_6delta_sierra3 },
    { "filter/0022_filter_dither_6delta_a_dither",
      test_filter_0022_filter_dither_6delta_a_dither },
    { "filter/0023_filter_dither_6delta_x_dither",
      test_filter_0023_filter_dither_6delta_x_dither },
    { "filter/0024_filter_dither_6delta_bluenoise",
      test_filter_0024_filter_dither_6delta_bluenoise },
    { "filter/0025_filter_dither_6delta_bluenoise_gpu",
      test_filter_0025_filter_dither_6delta_bluenoise_gpu },
    { "filter/0026_filter_dither_6delta_diffusion_skip",
      test_filter_0026_filter_dither_6delta_diffusion_skip },
    { "filter/0027_filter_gpu_6delta_disabled_reject",
      test_filter_0027_filter_gpu_6delta_disabled_reject },
    { "filter/0028_filter_dither_6delta_bluenoise_gpu_exact_plane",
      test_filter_0028_filter_dither_6delta_bluenoise_gpu_exact_plane },
    { "filter/0029_filter_dither_6delta_bluenoise_gpu_auto_fallback",
      test_filter_0029_dither_6delta_bluenoise_gpu_auto_fallback },
    { "filter/0030_filter_palette_failure_output",
      test_filter_0030_filter_palette_failure_output },
    { "filter/0031_filter_binning",
      test_filter_0031_filter_binning },
    { "filter/0032_filter_binning_failure_output",
      test_filter_0032_filter_binning_failure_output },
    { "filter/0033_filter_binning_exact",
      test_filter_0033_filter_binning_exact },
    { "filter/0034_filter_binning_sample_stream",
      test_filter_0034_filter_binning_sample_stream },
    { "filter/0035_filter_binning_stream_format",
      test_filter_0035_filter_binning_stream_format },
    { "filter/0036_filter_binning_stream_transparency",
      test_filter_0036_filter_binning_stream_transparency },
    { "filter/0037_filter_binning_stream_float",
      test_filter_0037_filter_binning_stream_float },
    { "filter/0038_filter_binning_stream_empty",
      test_filter_0038_filter_binning_stream_empty },
    { "filter/0039_filter_binning_stream_none",
      test_filter_0039_filter_binning_stream_none },
    { "filter/0040_filter_binning_stream_exact",
      test_filter_0040_filter_binning_stream_exact },
    { "filter/0041_filter_binning_uniform_256",
      test_filter_0041_filter_binning_uniform_256 },
    { "filter/0042_filter_binning_stream_alpha_zero",
      test_filter_0042_filter_binning_stream_alpha_zero },
    { "filter/0043_filter_dither_transparent_mask_fence_serial",
      test_filter_0043_filter_dither_transparent_mask_fence_serial },
    { "filter/0044_filter_dither_transparent_mask_fence_parallel",
      test_filter_0044_filter_dither_transparent_mask_fence_parallel },
    { "filter/0045_filter_binning_stream_alpha_zero_disabled",
      test_filter_0045_filter_binning_stream_alpha_zero_disabled },
    { "gpu-palette/0001_gpu_palette_threshold_strtoul",
      test_gpupal_0001_strtoul },
    { "gpu-palette/0002_reserved_key", test_gpupal_0002_reserved_key },
    { "gpu-palette/0003_fade_history", test_gpupal_0003_fade_history },
    { "gpu-palette/0004_auto_strength", test_gpupal_0004_auto_strength },
    { "gpu-palette/0005_wire_rgb_none",
      test_gpupal_0005_wire_rgb_none },
    { "gpu-palette/0006_wire_rgb_bluenoise",
      test_gpupal_0006_wire_rgb_bluenoise },
    { "gpu-palette/0007_wire_rgb_eytzinger",
      test_gpupal_0007_wire_rgb_eytzinger },
    { "gpu-palette/0008_wire_hls_none",
      test_gpupal_0008_wire_hls_none },
    { "gpu-palette/0009_wire_hls_bluenoise",
      test_gpupal_0009_wire_hls_bluenoise },
    { "gpu-palette/0010_comparison_default",
      test_gpupal_0010_comparison_default },
    { "gpu-palette/0011_comparison_short",
      test_gpupal_0011_comparison_short },
    { "dither/positional/0048_gpu_6delta", test_positional_0048_gpu_6delta },
    { "dither/bluenoise/0052_keep_offset", test_bluenoise_0052_keep_offset },
    { "gpu-dequant/0001_gpu_dequant_threshold_strtoul",
      test_gpudeq_0001_strtoul },
    { "security/0001_issue222_encoder_encode_bytes_packed_g1",
      test_security_0001_issue222_encoder_encode_bytes_packed_g1 },
    { "encoder-core/0001_encoder_core_factory",
      test_encoder_core_0001_encoder_core_factory },
    { "encoder-core/0027_ormode_public_output_disable",
      test_or_enc_0027 },
    { "encoder-core/0028_ormode_pipeline_size",
      test_or_enc_0028 },
    { "encoder-core/0029_ormode_offset_disables_pipeline",
      test_or_enc_0029 },
    { "decoder/0041_ormode_legacy_zero_is_color",
      test_or_dec_0041 },
    { "decoder/0042_default_palette_direct",
      test_pmap_0042 },
    { "decoder/0043_palette_map_definitions",
      test_pmap_0043 },
    { "decoder/0044_palette_map_redefinition",
      test_pmap_0044 },
    { "decoder/0045_palette_map_ormode",
      test_pmap_0045 },
    { "decoder/0046_palette_map_parallel",
      test_pmap_0046 },
    { "decoder/0047_palette_map_dequant",
      test_pmap_0047 },
    { "decoder/0048_palette_map_invalid",
      test_pmap_0048 },
    { "decoder/0049_palette_map_clip_background",
      test_pmap_0049 },
    { "decoder/0050_palette_map_identity",
      test_pmap_0050 },
    { "decoder/0051_palette_map_gpu",
      test_pmap_0051 },
    { "decoder/0052_palette_map_parallel_fallback",
      test_pmap_0052 },
    { "encoder-core/0007_ormode_one_color_plane",
      test_or_enc_0007 },
    { "encoder-core/0008_ormode_two_color_plane",
      test_or_enc_0008 },
    { "encoder-core/0009_ormode_three_color_planes",
      test_or_enc_0009 },
    { "encoder-core/0010_ormode_four_color_planes",
      test_or_enc_0010 },
    { "encoder-core/0011_ormode_five_color_planes",
      test_or_enc_0011 },
    { "encoder-core/0012_ormode_128_color_planes",
      test_or_enc_0012 },
    { "encoder-core/0013_ormode_129_color_high_bit",
      test_or_enc_0013 },
    { "encoder-core/0014_ormode_256_color_all_bits",
      test_or_enc_0014 },
    { "encoder-core/0015_ormode_size_zero_full_band",
      test_or_enc_0015 },
    { "encoder-core/0016_ormode_size_zero_tail_band",
      test_or_enc_0016 },
    { "encoder-core/0017_ormode_size_leading_empty_columns",
      test_or_enc_0017 },
    { "encoder-core/0018_ormode_size_tail_empty_plane",
      test_or_enc_0018 },
    { "encoder-core/0019_ormode_size_empty_then_nonempty_band",
      test_or_enc_0019 },
    { "encoder-core/0020_ormode_size_sample_last_column",
      test_or_enc_0020 },
    { "encoder-core/0021_ormode_size_sample_beyond_column",
      test_or_enc_0021 },
    { "encoder-core/0022_ormode_offset_top_0",
      test_or_enc_0022 },
    { "encoder-core/0023_ormode_offset_top_1",
      test_or_enc_0023 },
    { "encoder-core/0024_ormode_offset_top_5",
      test_or_enc_0024 },
    { "encoder-core/0025_ormode_offset_top_6",
      test_or_enc_0025 },
    { "encoder-core/0026_ormode_offset_top_7",
      test_or_enc_0026 },
    { "decoder/0022_ormode_selective_blur_opaque",
      test_or_dec_0022 },
    { "decoder/0027_ormode_kundither_opaque",
      test_or_dec_0027 },
    { "decoder/0028_ormode_repeated_bit_idempotent",
      test_or_dec_0028 },
    { "decoder/0029_ormode_plane_order_independent",
      test_or_dec_0029 },
    { "decoder/0030_ormode_zero_mask_retains_bits",
      test_or_dec_0030 },
    { "decoder/0031_ormode_high_bit_composition",
      test_or_dec_0031 },
    { "decoder/0032_ormode_p2_without_p1",
      test_or_dec_0032 },
    { "decoder/0033_ormode_p1_without_p2",
      test_or_dec_0033 },
    { "decoder/0034_ormode_parallel_rgba",
      test_or_dec_0034 },
    { "decoder/0035_ormode_parallel_indexed",
      test_or_dec_0035 },
    { "decoder/0036_ormode_fallback_rgba",
      test_or_dec_0036 },
    { "decoder/0037_ormode_fallback_indexed",
      test_or_dec_0037 },
    { "decoder/0038_ormode_parallel_indexed_request",
      test_or_dec_0038 },
    { "decoder/0039_ormode_decoder_reuse_resets_mode",
      test_or_dec_0039 },
    { "decoder/0040_ormode_direct_zero_is_opaque",
      test_or_dec_0040 },
    { "palette/or_offset_cli", test_or_offset_cli },
    { "encoder-core/0002_encoder_core_ormode_body_full_band",
      test_encoder_core_0002_encoder_core_ormode_body_full_band },
    { "encoder-core/0003_encoder_core_ormode_body_tail_band",
      test_encoder_core_0003_encoder_core_ormode_body_tail_band },
    { "encoder-core/0004_encoder_core_ormode_body_bad_argument",
      test_encoder_core_0004_encoder_core_ormode_body_bad_argument },
    { "encoder-core/0005_encoder_core_ormode_pipeline_matches_body",
      test_encoder_core_0005_ormode_pipeline_body_match },
    { "encoder-core/0006_encoder_core_ormode_body_skips_empty_planes",
      test_encoder_core_0006_ormode_body_skip_empty_planes },
    { "sixel-writer/0001_sixel_writer_factory",
      test_sixel_writer_0001_sixel_writer_factory },
    { "frame/0001_frame_factory", test_frame_0001_frame_factory },
    { "frame/0002_frame_float_request", test_frame_0002_float_request },
    { "frame/0006_extended_coordinates", test_frame_0006_extended },
    { "frame/0014_clone_float_canonical", test_frame_0014_clone_float },
    { "frame/0015_float_failure_atomic", test_frame_0015_atomic },
    { "frame/0018_float_resampling", test_frame_0018_resampling },
    { "frame/0054_float_geometry_state", test_frame_0054_geometry },
    { "frame/0098_float_dynamic_range", test_frame_0098_dynamic },
    { "frame/0178_float_byte_boundary", test_frame_0178_byte },
    { "apng/0001_float", test_apng_0001_float },
    { "frame/0239_alpha", test_frame_0239_alpha },
    { "frame/0003_frame_transparent_mask_clip",
      test_frame_0003_transparent_mask_clip },
    { "frame/0004_frame_transparent_mask_resize",
      test_frame_0004_transparent_mask_resize },
    { "frame/0005_frame_transparent_mask_resize_bilinear",
      test_frame_0005_transparent_mask_resize_bilinear },
    { "geometry/resampling_exact", test_geometry_resampling_exact },
    { "geometry/resampling_measurement_baseline",
      test_geometry_resampling_measurement_baseline },
    { "planner/0001_resize_linear_float32",
      test_plan_0001_resize_linear_float32 },
    { "planner/0002_resize_allocation_failure",
      test_plan_0002_resize_alloc_failure },
    { "precision/0001_byte_request_float_source", test_precision_0001 },
    { "precision/0002_byte_request_linear_source", test_precision_0002 },
    { "precision/0003_auto_float_source", test_precision_0003 },
    { "precision/0004_non_gamma_float_work", test_precision_0004 },
    { "precision/0005_resize_float_override", test_precision_0005 },
    { "precision/0006_resize_linear_float_source", test_precision_0006 },
    { "precision/0007_resize_preserve_float_source", test_precision_0007 },
    { "precision/0008_explicit_float_preserve", test_precision_0008 },
    { "precision/0009_dither_promotion", test_precision_0009 },
    { "precision/0018_packed_palette", test_precision_0018 },
    { "chunk/0001_chunk_factory", test_chunk_0001_chunk_factory },
    { "chunk/0002_chunk_cancel_callback", test_chunk_cancel_callback },
    { "timeline/0001_timeline_logger_factory",
      test_timeline_0001_timeline_logger_factory },
    { "timeline/0002_timeline_parallel_encode_decode",
      test_timeline_0002_timeline_parallel_encode_decode },
    { "timeline/0003_timeline_clock_origin",
      test_timeline_0003_timeline_clock_origin },
    { "timeline/0004_timeline_log_path_environment_probe",
      test_timeline_0004_timeline_log_path_environment_probe },
    { "timeline/0005_timeline_line_policy_environment",
      test_timeline_0005_timeline_line_policy_environment },
    { "platform/path/0001_path_to_libc_runtime",
      test_platform_path_0001_path_to_libc_runtime },
    { "threadpool/0001_threadpool_service",
      test_threadpool_0001_threadpool_service },
    { "threadpool/0002_thread_default_counts",
      test_threadpool_0002_thread_default_counts },
    { "scale/0001_parallel_factor_environment",
      test_scale_0001_parallel_factor_environment },
    { "scale/0002_parallel_min_bytes_environment",
      test_scale_0002_parallel_min_bytes_environment },
    { "scale/0003_resample_allocation_failure",
      test_scale_0003_resample_alloc_failure },
    { "colorspace/0001_din99d_forward", test_color_0001_din99d_forward },
    { "colorspace/0002_din99d_inverse", test_color_0002_din99d_inverse },
    { "colorspace/0003_din99d_byte", test_color_0003_din99d_byte },
    { "decoder/0053_percentage_rgb8", test_pmap_0053 },
    { "colorspace/0012_extended_failure_atomic", test_color_0012 },
    { "colorspace/0013_black_log1p_model", test_color_0013 },
    { "colorspace/public_float", test_color_public_float },
    { "colorspace/public_float_fault", test_color_public_float_fault },
    { "jpeg-cmyk/gray16-source", test_jp_gray16 },
    { "jpeg-cmyk/prefix-failure", test_jp_fault },
    { "jpeg-cmyk/precision", test_jc_precision },
    { "jpeg-cmyk/0001_affine_adobe_builtin", test_jc_0001 },
    { "jpeg-cmyk/0002_affine_plain_builtin", test_jc_0002 },
    { "jpeg-cmyk/0003_affine_adobe_lcms2", test_jc_0003 },
    { "jpeg-cmyk/0004_affine_plain_lcms2", test_jc_0004 },
    { "jpeg-cmyk/0005_affine_adobe_colorsync", test_jc_0005 },
    { "jpeg-cmyk/0006_affine_plain_colorsync", test_jc_0006 },
    { "jpeg-cmyk/0007_cms_disabled_adobe", test_jc_0007 },
    { "jpeg-cmyk/0008_cms_disabled_plain", test_jc_0008 },
    { "jpeg-cmyk/0009_profile_absent_adobe", test_jc_0009 },
    { "jpeg-cmyk/0010_profile_absent_plain", test_jc_0010 },
    { "jpeg-cmyk/0011_profile_rejected_adobe", test_jc_0011 },
    { "jpeg-cmyk/0012_profile_rejected_plain", test_jc_0012 },
    { "jpeg-cmyk/0013_apply_failure_adobe", test_jc_0013 },
    { "jpeg-cmyk/0014_apply_failure_plain", test_jc_0014 },
    { "jpeg-cmyk/0015_device_source", test_jc_0015 },
    { "jpeg-cmyk/0016_legacy_default_values", test_jc_0016 },
    { "jpeg-cmyk/0017_legacy_strict_apply_failure", test_jc_0017 },
    { "jpeg-cmyk/0018_legacy_lcms2_rejection", test_jc_0018 },
    { "jpeg-cmyk/0019_legacy_colorsync_rejection", test_jc_0019 },
    { "png-keycolor/0001_frame_samples", test_png_key_0001 },
    { "png-keycolor/0002_paint_mask", test_png_key_0002 },
    { "cms/0008_invalid_rgb_definition", test_cdef_0008 },
    { "cms/0009_icc_length_ownership", test_cdef_0009 },
    { "cms/0010_p3_float_roundtrip", test_cdef_0010 },
    { "cms/0011_separate_intents", test_cdef_0011 },
    { "cms/0012_engine_independence", test_cdef_0012 },
    { "cms/0013_queued_transform_lifetime", test_cdef_0013 },
    { "cms/0014_colorsync_linear_primaries", test_cms_0014 },
    { "cms/0015_builtin_float_samples", test_cms_0015 },
    { "cms/0016_builtin_extended_transfer", test_cms_0016 },
    { "cms/0017_builtin_byte_to_float", test_cms_0017 },
    { "cms/0018_gamma_decode_extended", test_cms_0018 },
    { "cms/0019_gamma_decode_invalid", test_cms_0019 },
    { "cms/0020_builtin_lut_physical_pcs", test_cms_0020 },
    { "cms/0021_builtin_inplace_expansion", test_cms_0021 },
    { "cms/0024_minimal_icc_acceptance", test_cms_0024 },
    { "cms/0025_integer_rgb_float", test_cms_0025 },
    { "cms/0033_integer_cmyk_float", test_cms_0033 },
    { "cms/0044_linear_failure_atomic", test_cms_0044 },
    { "cms/0045_linear_gray_samples", test_cms_0045 },
    { "cms/0053_colorsync_rgb_power", test_cms_0053 },
    { "cms/0128_colorsync_rgb_profile_reuse", test_cms_0128 },
    { "cms/0129_colorsync_rgb_backend_mismatch", test_cms_0129 },
    { "cms/0130_colorsync_rgb_passthrough", test_cms_0130 },
    { "cms/0146_curv_encoding", test_cms_0146 },
    { "cms/0147_colorsync_curv", test_cms_0147 },
    { "cms/0181_gamma_option", test_cms_0181 },
    { "cms/0195_native_gamma", test_cms_0195 },
    { "assessment/0001_ciede2000_reference", test_qa_0001 },
    { "assessment/0002_same_physical_coordinates", test_qa_0002 },
    { "assessment/0003_same_rgb_different_definition", test_qa_0003 },
    { "assessment/0004_p3_clipping_negative_control", test_qa_0004 },
    { "assessment/0005_region_physical_average", test_qa_0005 },
    { "assessment/0006_region_spatial_cancellation", test_qa_0006 },
    { "assessment/0007_region_partial_edge", test_qa_0007 },
    { "assessment/0008_frame_linear_lab", test_qa_0008 },
    { "assessment/0009_frame_linear_oklab", test_qa_0009 },
    { "assessment/0010_frame_linear_din99d", test_qa_0010 },
    { "assessment/0011_spec_parse_valid", test_qa_0011 },
    { "assessment/0012_spec_parse_invalid", test_qa_0012 },
    { "assessment/0013_pixel_delta_peak", test_qa_0013 },
    { "assessment/0017_ms_ssim_origin", test_qa_0017 },
    { "loader/image_values", test_image_values },
    { "loader/image_display_values", test_image_values },
    { "loader/image_black_values", test_image_values },
    { "loader/image_white_values", test_image_values },
    { "loader/image_sample_range", test_image_sample_range },
    { "loader/image_dimensions", test_image_dimensions },
    { "quant/palette/named_output/0037_adobe_libpng", test_named_output_0037 },
    { "quant/palette/named_output/0038_dci_libpng", test_named_output_0038 },
    { "quant/palette/named_output/0039_adobe_libtiff", test_named_output_0039 },
    { "quant/palette/named_output/0040_dci_libtiff", test_named_output_0040 },
    { "quant/palette/named_output/0001_adobe_gamma", test_named_output_0001 },
    { "quant/palette/named_output/0002_adobe_linear", test_named_output_0002 },
    { "quant/palette/named_output/0003_adobe_oklab", test_named_output_0003 },
    { "quant/palette/named_output/0004_adobe_cielab", test_named_output_0004 },
    { "quant/palette/named_output/0005_adobe_din99d", test_named_output_0005 },
    { "quant/palette/named_output/0006_adobe_center", test_named_output_0006 },
    { "quant/palette/named_output/0007_adobe_medoids", test_named_output_0007 },
    { "quant/palette/named_output/0008_adobe_heckbert",
      test_named_output_0008 },
    { "quant/palette/named_output/0009_adobe_highcolor",
      test_named_output_0009 },
    { "quant/palette/named_output/0010_adobe_reuse", test_named_output_0010 },
    { "quant/palette/named_output/0011_adobe_snap_zero",
      test_named_output_0011 },
    { "quant/palette/named_output/0012_adobe_option_atomicity",
      test_named_output_0012 },
    { "quant/palette/named_output/0013_adobe_xyz", test_named_output_0013 },
    { "quant/palette/named_output/0014_adobe_icc_export",
      test_named_output_0014 },
    { "quant/palette/named_output/0015_dci_gamma", test_named_output_0015 },
    { "quant/palette/named_output/0016_dci_linear", test_named_output_0016 },
    { "quant/palette/named_output/0017_dci_oklab", test_named_output_0017 },
    { "quant/palette/named_output/0018_dci_cielab", test_named_output_0018 },
    { "quant/palette/named_output/0019_dci_din99d", test_named_output_0019 },
    { "quant/palette/named_output/0020_dci_center", test_named_output_0020 },
    { "quant/palette/named_output/0021_dci_medoids", test_named_output_0021 },
    { "quant/palette/named_output/0022_dci_heckbert", test_named_output_0022 },
    { "quant/palette/named_output/0023_dci_highcolor", test_named_output_0023 },
    { "quant/palette/named_output/0024_dci_reuse", test_named_output_0024 },
    { "quant/palette/named_output/0025_dci_snap_zero",
      test_named_output_0025 },
    { "quant/palette/named_output/0026_dci_option_atomicity",
      test_named_output_0026 },
    { "quant/palette/named_output/0027_dci_xyz", test_named_output_0027 },
    { "quant/palette/named_output/0028_dci_icc_export",
      test_named_output_0028 },
    { "quant/palette/named_output/0029_adobe_delta_switch",
      test_named_output_0029 },
    { "quant/palette/named_output/0030_adobe_hls", test_named_output_0030 },
    { "quant/palette/named_output/0031_adobe_nan_recovery",
      test_named_output_0031 },
    { "quant/palette/named_output/0032_dci_delta_switch",
      test_named_output_0032 },
    { "quant/palette/named_output/0033_dci_hls", test_named_output_0033 },
    { "quant/palette/named_output/0034_dci_nan_recovery",
      test_named_output_0034 },
    { "writer/png/0030_adobe_rgb_snapshot", test_png_color_0030 },
    { "writer/png/0031_dci_p3_snapshot", test_png_color_0031 },
    { "writer/png/0026_adobe_rgb_indexed", test_png_color_0026 },
    { "writer/png/0027_dci_p3_indexed", test_png_color_0027 },
    { "writer/png/0028_adobe_rgb_direct", test_png_color_0028 },
    { "writer/png/0029_dci_p3_direct", test_png_color_0029 },
    { "cms/0001_named_xyz", test_cdef_0001 },
    { "cms/0002_simple_power", test_cdef_0002 },
    { "cms/0003_icc_source_lifetime", test_cdef_0003 },
    { "cms/0004_transform_owns_endpoints", test_cdef_0004 },
    { "cms/0005_matrix_icc_output", test_cdef_0005 },
    { "cms/0006_forward_lut_rejected_output", test_cdef_0006 },
    { "cms/0007_paired_lut_applies", test_cdef_0007 },
    { "icc/0009_parametric_type4", test_icc_0009 },
    { "icc/0010_para_nan", test_icc_0010 },
    { "icc/0011_para_overflow", test_icc_0011 },
    { "icc/0012_segm_nan", test_icc_0012 },
    { "icc/0013_segm_overflow", test_icc_0013 },
    { "icc/0014_para_finite_table", test_icc_0014 },
    { "icc/0015_segm_finite_table", test_icc_0015 },
    { "icc/0016_para_loader_recovery", test_icc_0016 },
    { "icc/0017_segm_loader_recovery", test_icc_0017 },
    { "colorspace/0004_extended_srgb_decode", test_color_0004 },
    { "colorspace/0005_extended_srgb_encode", test_color_0005 },
    { "colorspace/0006_extended_oklab_forward", test_color_0006 },
    { "colorspace/0007_extended_oklab_inverse", test_color_0007 },
    { "colorspace/0008_extended_lab_forward", test_color_0008 },
    { "colorspace/0009_extended_lab_inverse", test_color_0009 },
    { "colorspace/0010_extended_din99d_forward", test_color_0010 },
    { "colorspace/0011_extended_din99d_inverse", test_color_0011 },
    { "scale/0004_g8_normalized_rgb888",
      test_scale_0004_g8_rgb888 },
    { "scale/0005_rgb565_normalized_rgb888",
      test_scale_0005_rgb565_rgb888 },
    { "scale/0006_rgba8888_normalized_rgb888",
      test_scale_0006_rgba_rgb888 },
    { "decoder/0001_decoder_parallel_split_after_newline",
      test_decoder_0001_decoder_parallel_split_after_newline },
    { "decoder/0002_decoder_ormode_raw_overlay",
      test_decoder_0002_decoder_ormode_raw_overlay },
    { "decoder/0003_decoder_ormode_wide_overlay",
      test_decoder_0003_decoder_ormode_wide_overlay },
    { "decoder/0004_decoder_ormode_direct_overlay",
      test_decoder_0004_decoder_ormode_direct_overlay },
    { "decoder/0005_decoder_ormode_repeat_overlay",
      test_decoder_0005_decoder_ormode_repeat_overlay },
    { "decoder/0006_decoder_ormode_legacy_api",
      test_decoder_0006_decoder_ormode_legacy_api },
    { "decoder/0007_decoder_ormode_parallel_request",
      test_decoder_0007_decoder_ormode_parallel_request },
    { "decoder/0008_decoder_pixels_output_formats",
      test_decoder_0008_decoder_pixels_output_formats },
    { "decoder/0009_decoder_pixels_trust_raster_clip",
      test_decoder_0009_decoder_pixels_trust_raster_clip },
    { "decoder/0010_decoder_kundither_default",
      test_decoder_0010_decoder_kundither_default },
    { "decoder/0011_decoder_kundither_similarity",
      test_decoder_0011_decoder_kundither_similarity },
    { "decoder/0012_decoder_kundither_edge",
      test_decoder_0012_decoder_kundither_edge },
    { "decoder/0013_decoder_kundither_parallel_matches_scalar",
      test_decoder_0013_decoder_kundither_parallel_matches_scalar },
    { "decoder/0014_decoder_kundither_fast4_parallel_matches_scalar",
      test_decoder_0014_kundither_fast4_matches_scalar },
    { "decoder/0015_decoder_kundither_fast4_fused_matches_scalar",
      test_decoder_0015_decoder_kundither_fast4_fused_matches_scalar },
    { "decoder/0016_decoder_parallel_fallback_keeps_image_clean",
      test_decoder_0016_decoder_parallel_fallback_keeps_image_clean },
    { "decoder/0017_decoder_parallel_sparse_split_matches_serial",
      test_decoder_0017_decoder_parallel_sparse_split_matches_serial },
    { "decoder/0018_decoder_parallel_direct_sparse_preserves_unpainted",
      test_decoder_0018_direct_sparse_unpainted },
    { "decoder/0019_decoder_parallel_direct_repeat_overflow_fallback",
      test_decoder_0019_direct_repeat_overflow },
    { "decoder/0020_decoder_pixels_body_api",
      test_decoder_0020_decoder_pixels_body_api },
    { "decoder/0021_decoder_selective_blur_threshold",
      test_decoder_0021_decoder_selective_blur_threshold },
    { "decoder/0023_decoder_high_color_dequantize_bypass",
      test_decoder_0023_decoder_high_color_dequantize_bypass },
    { "decoder/0024_decoder_parallel_skew_environment",
      test_decoder_0024_decoder_parallel_skew_environment },
    { "decoder/0025_decoder_paint_create_failure_clean",
      test_decoder_0025_decoder_paint_create_failure_clean },
    { "decoder/0026_decoder_paint_overlap_fallback_clean",
      test_decoder_0026_decoder_paint_overlap_fallback_clean },
    { "dither/0001_dither_policy", test_dither_0001_dither_policy },
    { "lookup/0001_lookup_policy",
      test_lookup_0001_lookup_policy },
    { "lookup/0012_lookup_packing_registry_direct",
      test_lookup_0012_packing_registry_direct },
    { "lookup/0013_parallel_dense_cache",
      test_lookup_0013_parallel_dense_cache },
    { "probe/0001_probe_parse", test_probe_0001_probe_parse },
    { "status/0001_status_timeout", test_status_0001_status_timeout },
    { "icc/0001_icc_builtin_rgb_gray_v4_paths",
      test_icc_0001_icc_builtin_rgb_gray_v4_paths },
    { "icc/0002_icc_builtin_mab_mba_a2b0_paths",
      test_icc_0002_icc_builtin_mab_mba_a2b0_paths },
    { "icc/0003_icc_builtin_a2b_intent_paths",
      test_icc_0003_icc_builtin_a2b_intent_paths },
    { "icc/0004_icc_builtin_b2a_slot_paths",
      test_icc_0004_icc_builtin_b2a_slot_paths },
    { "icc/0005_icc_builtin_device_to_device_intent_paths",
      test_icc_0005_icc_builtin_device_to_device_intent_paths },
    { "icc/0006_icc_builtin_pixel_format_matrix",
      test_icc_0006_icc_builtin_pixel_format_matrix },
    { "icc/0007_icc_builtin_float_special_values",
      test_icc_0007_icc_builtin_float_special_values },
    { "icc/0008_icc_builtin_d2b_b2d_legacy_lut",
      test_icc_0008_icc_builtin_d2b_b2d_legacy_lut },
    { "palette/0001_kmeans_init", test_palette_0001_kmeans_init },
    { "palette/0002_kmedoids_constraints",
      test_palette_0002_kmedoids_constraints },
    { "palette/0003_kcenter_constraints",
      test_palette_0003_kcenter_constraints },
    { "palette/0004_palette_factory", test_palette_0004_palette_factory },
    { "palette/0005_cover_anchor", test_palette_0005_cover_anchor },
    { "palette/fallback/0006_palette_fallback_conversion",
      test_palfb_0006_conversion },
    { "palette/fallback/0007_palette_fallback_output",
      test_palfb_0007_output },
    { "palette/0008_palette_policy_resolution",
      test_palette_0008_palette_policy_resolution },
    { "palette/0025_sample_stream", test_palette_0025_sample_stream },
    { "palette/0026_sampling_auto_resolution",
      test_palette_0026_sampling_auto },
    { "palette/0028_sampling_scheduler_contract",
      test_palette_0028_sampling_schedule },
    { "palette/0029_sampling_fallback_resolution",
      test_palette_0029_sampling_fallback },
    { "palette/0030_binning_policy_contract",
      test_palette_0030_binning_policy },
    { "palette/0031_weighted_point_set",
      test_palette_0031_weighted_point_set },
    { "palette/0032_kmeans_binning_policy_output",
      test_palette_0032_kmeans_binning_policy_output },
    { "palette/0033_quantizer_capabilities",
      test_palette_0033_quantizer_capabilities },
    { "palette/0034_palette_stage_resolvers",
      test_palette_0034_palette_stage_resolvers },
    { "palette/0037_palette_sampling_indexed_frame",
      test_palette_0037_palette_sampling_indexed_frame },
    { "palette/0040_palette_build_attempt_isolation",
      test_palette_0040_palette_build_attempt_isolation },
    { "palette/0041_kcenter_binning_filter",
      test_palette_0041_kcenter_binning_filter },
    { "palette/0042_kcenter_large_buffer",
      test_palette_0042_kcenter_large_buffer },
    { "palette/0043_kcenter_float_grid_values",
      test_palette_0043_kcenter_float_grid_values },
    { "palette/0044_kcenter_packed_alpha_layouts",
      test_palette_0044_kcenter_packed_alpha_layouts },
    { "palette/0045_snap_output_grid", test_palette_0045_snap_grid },
    { "palette/0048_snap_partial_once", test_palette_0048_snap_partial },
    { "cli/0029_cli_token_is_known_option",
      test_cli_0029_cli_token_is_known_option },
    { "cli/0030_cli_option_requires_argument",
      test_cli_0030_cli_option_requires_argument },
    { "cli/0031_cli_guard_missing_argument",
      test_cli_0031_cli_guard_missing_argument },
    { "cli/0032_cli_gpu_policy_option",
      test_cli_0032_cli_gpu_policy_option },
    { "cli/0033_cli_suboption_consumer_scope",
      test_cli_0033_consumer_scope },
    { "cli/0034_cli_terminal_policy_option",
      test_cli_0034_cli_terminal_policy_option },
    { "cli/0035_cli_lookup_policy_initial_prefixes",
      test_cli_0035_lookup_initials },
    { "cli/0036_cli_color_wire", test_cli_0036_color_wire },
    { "cli/0037_cli_color_parser", test_cli_0037_color_parser },
    { "cli/0038_cli_color_tty", test_cli_0038_color_tty },
    { "cli/0039_cli_color_auto", test_cli_0039_color_auto },
    { "cli/0040_cli_color_job", test_cli_0040_color_job },
    { "cli/core/0033_stdin_interrupt", test_cli_stdin_interrupt },
    { "palette/color_space/0001_space_contract", test_palette_space },
    { "palette/binning_float/0001_binning_contract",
      test_palette_binning_float },
    { "lookup/extended/0001_extended_lookup", test_extended_lookup },
    {"dither/positional/precision/0059_a_byte_band",
     test_positional_0059},
    {"dither/positional/precision/0060_x_byte_band",
     test_positional_0060},
    {"dither/positional/precision/0061_blue_byte_band",
     test_positional_0061},
    {"dither/positional/precision/0076_blue_strength_override",
     test_positional_0076},
    {"dither/positional/precision/0075_x_strength_override",
     test_positional_0075},
    {"dither/positional/precision/0074_a_strength_override",
     test_positional_0074},
    {"dither/positional/precision/0073_blue_strength_env",
     test_positional_0073},
    {"dither/positional/precision/0072_x_strength_env",
     test_positional_0072},
    {"dither/positional/precision/0071_a_strength_env",
     test_positional_0071},
    {"dither/positional/precision/0070_blue_auto_byte",
     test_positional_0070},
    {"dither/positional/precision/0069_x_auto_byte",
     test_positional_0069},
    {"dither/positional/precision/0068_a_auto_byte",
     test_positional_0068},
    {"dither/positional/precision/0067_blue_auto_float",
     test_positional_0067},
    {"dither/positional/precision/0066_x_auto_float",
     test_positional_0066},
    {"dither/positional/precision/0065_a_auto_float",
     test_positional_0065},
    {"dither/positional/precision/0001_a_zero_gamma",
     test_positional_0001},
    {"dither/positional/precision/0002_a_zero_linear",
     test_positional_0002},
    {"dither/positional/precision/0003_a_zero_oklab",
     test_positional_0003},
    {"dither/positional/precision/0004_a_zero_cielab",
     test_positional_0004},
    {"dither/positional/precision/0005_a_zero_din99d",
     test_positional_0005},
    {"dither/positional/precision/0006_a_bias_gamma",
     test_positional_0006},
    {"dither/positional/precision/0007_a_bias_lab",
     test_positional_0007},
    {"dither/positional/precision/0008_a_byte_saturation",
     test_positional_0008},
    {"dither/positional/precision/0009_a_absolute_band",
     test_positional_0009},
    {"dither/positional/precision/0010_a_large_coordinates",
     test_positional_0010},
    {"dither/positional/precision/0011_a_reprepare",
     test_positional_0011},
    {"dither/positional/precision/0012_x_zero_gamma",
     test_positional_0012},
    {"dither/positional/precision/0013_x_zero_linear",
     test_positional_0013},
    {"dither/positional/precision/0014_x_zero_oklab",
     test_positional_0014},
    {"dither/positional/precision/0015_x_zero_cielab",
     test_positional_0015},
    {"dither/positional/precision/0016_x_zero_din99d",
     test_positional_0016},
    {"dither/positional/precision/0017_x_bias_gamma",
     test_positional_0017},
    {"dither/positional/precision/0018_x_bias_lab",
     test_positional_0018},
    {"dither/positional/precision/0019_x_byte_saturation",
     test_positional_0019},
    {"dither/positional/precision/0020_x_absolute_band",
     test_positional_0020},
    {"dither/positional/precision/0021_x_large_coordinates",
     test_positional_0021},
    {"dither/positional/precision/0022_x_reprepare",
     test_positional_0022},
    {"dither/positional/precision/0023_blue_zero_gamma",
     test_positional_0023},
    {"dither/positional/precision/0024_blue_zero_linear",
     test_positional_0024},
    {"dither/positional/precision/0025_blue_zero_oklab",
     test_positional_0025},
    {"dither/positional/precision/0026_blue_zero_cielab",
     test_positional_0026},
    {"dither/positional/precision/0027_blue_zero_din99d",
     test_positional_0027},
    {"dither/positional/precision/0028_blue_bias_gamma",
     test_positional_0028},
    {"dither/positional/precision/0029_blue_bias_lab",
     test_positional_0029},
    {"dither/positional/precision/0030_blue_byte_saturation",
     test_positional_0030},
    {"dither/positional/precision/0031_blue_absolute_band",
     test_positional_0031},
    {"dither/positional/precision/0032_blue_large_coordinates",
     test_positional_0032},
    {"dither/positional/precision/0033_blue_reprepare",
     test_positional_0033},
    {"dither/positional/precision/0034_blue_gradient_endpoints",
     test_positional_0034},
    {"dither/positional/precision/0035_a_encoder_gamma",
     test_positional_0035},
    {"dither/positional/precision/0036_a_encoder_linear",
     test_positional_0036},
    {"dither/positional/precision/0037_a_encoder_oklab",
     test_positional_0037},
    {"dither/positional/precision/0038_a_encoder_cielab",
     test_positional_0038},
    {"dither/positional/precision/0039_a_encoder_din99d",
     test_positional_0039},
    {"dither/positional/precision/0040_a_encoder_p3",
     test_positional_0040},
    {"dither/positional/precision/0041_a_encoder_byte",
     test_positional_0041},
    {"dither/positional/precision/0042_a_encoder_workers",
     test_positional_0042},
    {"dither/positional/precision/0043_x_encoder_gamma",
     test_positional_0043},
    {"dither/positional/precision/0044_x_encoder_linear",
     test_positional_0044},
    {"dither/positional/precision/0045_x_encoder_oklab",
     test_positional_0045},
    {"dither/positional/precision/0046_x_encoder_cielab",
     test_positional_0046},
    {"dither/positional/precision/0047_x_encoder_din99d",
     test_positional_0047},
    {"dither/positional/precision/0048_x_encoder_p3",
     test_positional_0048},
    {"dither/positional/precision/0049_x_encoder_byte",
     test_positional_0049},
    {"dither/positional/precision/0050_x_encoder_workers",
     test_positional_0050},
    {"dither/positional/precision/0051_blue_encoder_gamma",
     test_positional_0051},
    {"dither/positional/precision/0052_blue_encoder_linear",
     test_positional_0052},
    {"dither/positional/precision/0053_blue_encoder_oklab",
     test_positional_0053},
    {"dither/positional/precision/0054_blue_encoder_cielab",
     test_positional_0054},
    {"dither/positional/precision/0055_blue_encoder_din99d",
     test_positional_0055},
    {"dither/positional/precision/0056_blue_encoder_p3",
     test_positional_0056},
    {"dither/positional/precision/0057_blue_encoder_byte",
     test_positional_0057},
    {"dither/positional/precision/0058_blue_encoder_workers",
     test_positional_0058},
    { "palette/map_precision/0001_map_cms_gamma",
      test_map_precision_0001 },
    { "palette/map_precision/0002_map_cms_linear",
      test_map_precision_0002 },
    { "palette/map_precision/0003_map_cms_oklab",
      test_map_precision_0003 },
    { "palette/map_precision/0004_map_cms_cielab",
      test_map_precision_0004 },
    { "palette/map_precision/0005_map_cms_din99d",
      test_map_precision_0005 },
    { "palette/map_precision/0006_map_cms_auto",
      test_map_precision_0006 },
    { "palette/map_precision/0007_map_cms_float",
      test_map_precision_0007 },
    { "palette/map_precision/0008_map_cms_gamma_target",
      test_map_precision_0008 },
    { "palette/map_precision/0009_map_cms_disabled_byte",
      test_map_precision_0009 },
    { "palette/map_precision/0010_map_cms_explicit_narrowing",
      test_map_precision_0010 },
    { "palette/map_precision/0011_map_untagged_byte",
      test_map_precision_0011 },
    { "palette/map_precision/0012_map_cms_auto_solver",
      test_map_precision_0012 },
    { "palette/map_precision/0013_map_cms_center",
      test_map_precision_0013 },
    { "palette/map_precision/0014_map_cms_medoids",
      test_map_precision_0014 },
    { "palette/map_precision/0015_map_reuse_byte_first",
      test_map_precision_0015 },
    { "palette/map_precision/0016_map_reuse_float_first",
      test_map_precision_0016 },
    { "palette/map_precision/0017_map_cms_export",
      test_map_precision_0017 },
    { "palette/map_precision/0018_map_cms_p3_control",
      test_map_precision_0018 },
    { "palette/map_precision/0019_map_multiframe_native",
      test_map_precision_0019 },
    { "palette/map_precision/0020_map_promote_fs",
      test_map_precision_0020 },
    { "palette/map_precision/0021_map_allocation_none",
      test_map_precision_0021 },
    { "palette/map_precision/0022_map_allocation_fs",
      test_map_precision_0022 },
    { "palette/map_precision/0023_map_static_to_animation",
      test_map_precision_0023 },
    { "palette/map_precision/0024_map_allocation_borrowed",
      test_map_precision_0024 },
    { "palette/map_solver/0001_map_heckbert_gamma",
      test_map_solver_0001 },
    { "palette/map_solver/0002_map_heckbert_linear",
      test_map_solver_0002 },
    { "palette/map_solver/0003_map_heckbert_oklab",
      test_map_solver_0003 },
    { "palette/map_solver/0004_map_heckbert_cielab",
      test_map_solver_0004 },
    { "palette/map_solver/0005_map_heckbert_din99d",
      test_map_solver_0005 },
    { "palette/map_solver/0006_map_center_gamma",
      test_map_solver_0006 },
    { "palette/map_solver/0007_map_center_linear",
      test_map_solver_0007 },
    { "palette/map_solver/0008_map_center_oklab",
      test_map_solver_0008 },
    { "palette/map_solver/0009_map_center_cielab",
      test_map_solver_0009 },
    { "palette/map_solver/0010_map_center_din99d",
      test_map_solver_0010 },
    { "palette/map_solver/0011_map_medoids_gamma",
      test_map_solver_0011 },
    { "palette/map_solver/0012_map_medoids_linear",
      test_map_solver_0012 },
    { "palette/map_solver/0013_map_medoids_oklab",
      test_map_solver_0013 },
    { "palette/map_solver/0014_map_medoids_cielab",
      test_map_solver_0014 },
    { "palette/map_solver/0015_map_medoids_din99d",
      test_map_solver_0015 },
    { "palette/map_solver/0016_map_auto_omitted",
      test_map_solver_0016 },
    { "palette/map_solver/0017_map_auto_none",
      test_map_solver_0017 },
    { "palette/map_solver/0018_map_auto_hard",
      test_map_solver_0018 },
    { "palette/map_solver/0019_map_auto_soft",
      test_map_solver_0019 },
    { "palette/map_solver/0020_map_auto_exact",
      test_map_solver_0020 },
    { "palette/map_solver/0021_map_p3_heckbert",
      test_map_solver_0021 },
    { "palette/map_solver/0022_map_p3_center",
      test_map_solver_0022 },
    { "palette/map_solver/0023_map_p3_medoids",
      test_map_solver_0023 },
    { "palette/map_solver/0024_map_p3_auto_none",
      test_map_solver_0024 },
    { "palette/map_solver/0025_map_p3_auto_hard",
      test_map_solver_0025 },
    { "palette/map_solver/0026_map_fixed_auto",
      test_map_solver_0026 },
    { "palette/map_solver/0027_map_fixed_heckbert",
      test_map_solver_0027 },
    { "palette/map_solver/0028_map_fixed_center",
      test_map_solver_0028 },
    { "palette/map_solver/0029_map_fixed_medoids",
      test_map_solver_0029 },
    { "palette/map_solver/0030_map_byte_auto",
      test_map_solver_0030 },
    { "palette/map_solver/0031_map_byte_heckbert",
      test_map_solver_0031 },
    { "palette/map_solver/0032_map_byte_center",
      test_map_solver_0032 },
    { "palette/map_solver/0033_map_byte_medoids",
      test_map_solver_0033 },
    { "palette/map_solver/0034_map_auto_reuse",
      test_map_solver_0034 },
    { "palette/map_solver/0035_map_invalid_binning",
      test_map_solver_0035 },
    { "palette/map_solver/0036_map_fixed_p3_auto",
      test_map_solver_0036 },
    { "palette/map_float/0001_map_palette_gamma",
      test_map_native_0001 },
    { "palette/map_float/0002_map_palette_linear",
      test_map_native_0002 },
    { "palette/map_float/0003_map_palette_oklab",
      test_map_native_0003 },
    { "palette/map_float/0004_map_palette_cielab",
      test_map_native_0004 },
    { "palette/map_float/0005_map_palette_din99d",
      test_map_native_0005 },
    { "palette/map_float/0006_map_fs_gamma",
      test_map_native_0006 },
    { "palette/map_float/0007_map_fs_linear",
      test_map_native_0007 },
    { "palette/map_float/0008_map_fs_oklab",
      test_map_native_0008 },
    { "palette/map_float/0009_map_fs_cielab",
      test_map_native_0009 },
    { "palette/map_float/0010_map_fs_din99d",
      test_map_native_0010 },
    { "palette/map_float/0011_map_act",
      test_map_native_0011 },
    { "palette/map_float/0012_map_riff",
      test_map_native_0012 },
    { "palette/map_float/0013_map_gpl",
      test_map_native_0013 },
    { "palette/map_float/0014_map_byte",
      test_map_native_0014 },
    { "palette/map_float/0015_map_promote",
      test_map_native_0015 },
    { "palette/map_float/0016_map_precision_reuse",
      test_map_native_0016 },
    { "palette/map_float/0017_map_output_reuse",
      test_map_native_0017 },
    { "palette/map_float/0018_map_image_gamma",
      test_map_native_0018 },
    { "palette/map_float/0019_map_image_linear",
      test_map_native_0019 },
    { "palette/map_float/0020_map_image_oklab",
      test_map_native_0020 },
    { "palette/map_float/0021_map_image_cielab",
      test_map_native_0021 },
    { "palette/map_float/0022_map_image_din99d",
      test_map_native_0022 },
    { "palette/map_float/0023_map_export_gamma",
      test_map_native_0023 },
    { "palette/map_float/0024_map_export_cielab",
      test_map_native_0024 },
    { "palette/map_float/0025_map_zero_snap",
      test_map_native_0025 },
    { "palette/auto_float/0001_auto_default_gamma",
      test_auto_native_0001 },
    { "palette/auto_float/0002_auto_explicit_gamma",
      test_auto_native_0002 },
    { "palette/auto_float/0003_auto_default_p3",
      test_auto_native_0003 },
    { "palette/auto_float/0004_auto_default_linear",
      test_auto_native_0004 },
    { "palette/auto_float/0005_auto_default_oklab",
      test_auto_native_0005 },
    { "palette/auto_float/0006_auto_default_cielab",
      test_auto_native_0006 },
    { "palette/auto_float/0007_auto_default_din99d",
      test_auto_native_0007 },
    { "palette/auto_float/0008_auto_retain_float",
      test_auto_native_0008 },
    { "palette/auto_float/0009_auto_byte_fastpath",
      test_auto_native_0009 },
    { "palette/auto_float/0010_auto_byte_promote",
      test_auto_native_0010 },
    { "palette/auto_float/0011_auto_exact_kmeans",
      test_auto_native_0011 },
    { "palette/auto_float/0012_auto_soft_kmeans",
      test_auto_native_0012 },
    { "palette/auto_float/0013_auto_zero_snap",
      test_auto_native_0013 },
    { "palette/auto_float/0014_auto_binning_reuse",
      test_auto_native_0014 },
    { "palette/auto_float/0015_auto_implicit_cover",
      test_auto_native_0015 },
    { "palette/auto_float/0016_auto_p3_snap",
      test_auto_native_0016 },
    { "palette/auto_float/0017_auto_subbyte_diffusion",
      test_auto_native_0017 },
    { "palette/auto_float/0018_auto_explicit_auto_binning",
      test_auto_native_0018 },
    { "palette/auto_float/0019_auto_byte_hard_kmeans",
      test_auto_native_0019 },
    { "palette/auto_float/0020_auto_hard_kmeans_p3",
      test_auto_native_0020 },
    { "palette/auto_float/0021_auto_explicit_p3",
      test_auto_native_0021 },
    { "palette/auto_float/0022_auto_reject_kmeans_feedback",
      test_auto_native_0022 },
    { "palette/auto_float/0023_auto_irrelevant_heckbert_feedback",
      test_auto_native_0023 },
    { "palette/heckbert_float/0033_heckbert_repeated_population",
      test_heckbert_native_0033 },
    { "palette/heckbert_float/0034_heckbert_repeated_subbyte",
      test_heckbert_native_0034 },
    { "palette/heckbert_float/0032_heckbert_subbyte_diffusion",
      test_heckbert_native_0032 },
    { "palette/heckbert_float/0030_heckbert_histogram_allocation",
      test_heckbert_native_0030 },
    { "palette/heckbert_float/0031_heckbert_representative_allocation",
      test_heckbert_native_0031 },
    { "palette/heckbert_float/0001_heckbert_center",
      test_heckbert_native_0001 },
    { "palette/heckbert_float/0002_heckbert_average",
      test_heckbert_native_0002 },
    { "palette/heckbert_float/0003_heckbert_histogram",
      test_heckbert_native_0003 },
    { "palette/heckbert_float/0004_heckbert_gamma_extended",
      test_heckbert_native_0004 },
    { "palette/heckbert_float/0005_heckbert_linear_extended",
      test_heckbert_native_0005 },
    { "palette/heckbert_float/0006_heckbert_oklab_extended",
      test_heckbert_native_0006 },
    { "palette/heckbert_float/0007_heckbert_cielab_extended",
      test_heckbert_native_0007 },
    { "palette/heckbert_float/0008_heckbert_din99d_extended",
      test_heckbert_native_0008 },
    { "palette/heckbert_float/0009_heckbert_subbyte",
      test_heckbert_native_0009 },
    { "palette/heckbert_float/0010_heckbert_tiny_signed",
      test_heckbert_native_0010 },
    { "palette/heckbert_float/0011_heckbert_finite_extrema",
      test_heckbert_native_0011 },
    { "palette/heckbert_float/0012_heckbert_range_axis",
      test_heckbert_native_0012 },
    { "palette/heckbert_float/0013_heckbert_luminance_axis",
      test_heckbert_native_0013 },
    { "palette/heckbert_float/0014_heckbert_pca_split",
      test_heckbert_native_0014 },
    { "palette/heckbert_float/0015_heckbert_ward",
      test_heckbert_native_0015 },
    { "palette/heckbert_float/0016_heckbert_ward_refine",
      test_heckbert_native_0016 },
    { "palette/heckbert_float/0017_heckbert_nan",
      test_heckbert_native_0017 },
    { "palette/heckbert_float/0018_heckbert_infinity",
      test_heckbert_native_0018 },
    { "palette/heckbert_float/0019_heckbert_ward_linear",
      test_heckbert_native_0019 },
    { "palette/heckbert_float/0020_heckbert_ward_oklab",
      test_heckbert_native_0020 },
    { "palette/heckbert_float/0021_heckbert_ward_cielab",
      test_heckbert_native_0021 },
    { "palette/heckbert_float/0022_heckbert_ward_din99d",
      test_heckbert_native_0022 },
    { "palette/heckbert_float/0023_heckbert_gamma_wire",
      test_heckbert_native_0023 },
    { "palette/heckbert_float/0024_heckbert_linear_wire",
      test_heckbert_native_0024 },
    { "palette/heckbert_float/0025_heckbert_oklab_wire",
      test_heckbert_native_0025 },
    { "palette/heckbert_float/0026_heckbert_cielab_wire",
      test_heckbert_native_0026 },
    { "palette/heckbert_float/0027_heckbert_din99d_wire",
      test_heckbert_native_0027 },
    { "palette/heckbert_float/0028_heckbert_p3_wire",
      test_heckbert_native_0028 },
    { "palette/heckbert_float/0029_heckbert_byte",
      test_heckbert_native_0029 },
    { "lookup/fhedt/0020_fh_enclosing_cell",
      test_fh_lookup_0020 },
    { "lookup/fhedt/0021_fh_collision_refine",
      test_fh_lookup_0021 },
    { "lookup/fhedt/0022_fh_collision_dist2",
      test_fh_lookup_0022 },
    { "lookup/fhedt/0023_fh_instance_isolation",
      test_fh_lookup_0023 },
    { "lookup/mono_float/0001_dark_values",
      test_mono_lookup_0001 },
    { "lookup/mono_float/0002_light_values",
      test_mono_lookup_0002 },
    { "lookup/mono_float/0003_padded_stride",
      test_mono_lookup_0003 },
    { "lookup/mono_float/0004_byte_palette",
      test_mono_lookup_0004 },
    { "lookup/mono_float/0005_reprepare_isolation",
      test_mono_lookup_0005 },
    { "lookup/mono_float/0006_extended_sum",
      test_mono_lookup_0006 },
    { "lookup/mono_float/0007_byte_compatibility",
      test_mono_lookup_0007 },
    { "lookup/mono_float/0008_encoder_dark",
      test_mono_lookup_0008 },
    { "lookup/mono_float/0009_encoder_light",
      test_mono_lookup_0009 },
    { "lookup/mono_float/0010_encoder_fs",
      test_mono_lookup_0010 },
    { "quant/palette/fixed_mask/0001_srgb_gamma",
      test_fixed_mask_0001 },
    { "quant/palette/fixed_mask/0002_srgb_linear",
      test_fixed_mask_0002 },
    { "quant/palette/fixed_mask/0003_srgb_oklab",
      test_fixed_mask_0003 },
    { "quant/palette/fixed_mask/0004_srgb_cielab",
      test_fixed_mask_0004 },
    { "quant/palette/fixed_mask/0005_srgb_din99d",
      test_fixed_mask_0005 },
    { "quant/palette/fixed_mask/0006_p3_gamma",
      test_fixed_mask_0006 },
    { "quant/palette/fixed_mask/0007_p3_linear",
      test_fixed_mask_0007 },
    { "quant/palette/fixed_mask/0008_p3_oklab",
      test_fixed_mask_0008 },
    { "quant/palette/fixed_mask/0009_p3_cielab",
      test_fixed_mask_0009 },
    { "quant/palette/fixed_mask/0010_p3_din99d",
      test_fixed_mask_0010 },
    { "quant/palette/fixed_mask/0011_full256",
      test_fixed_mask_0011 },
    { "quant/palette/fixed_mask/0012_fs_feedback",
      test_fixed_mask_0012 },
    { "quant/palette/fixed_mask/0013_fhedt",
      test_fixed_mask_0013 },
    { "quant/palette/fixed_mask/0014_parallel_bands",
      test_fixed_mask_0014 },
    { "quant/palette/fixed_mask/0015_offset_bands",
      test_fixed_mask_0015 },
    { "quant/palette/fixed_mask/0016_masked_opaque_reuse",
      test_fixed_mask_0016 },
    { "quant/palette/fixed_mask/0017_all_hidden",
      test_fixed_mask_0017 },
    { "quant/palette/fixed_mask/0018_empty_mask",
      test_fixed_mask_0018 },
    { "quant/palette/fixed_mask/0019_clear_mask",
      test_fixed_mask_0019 },
    { "quant/palette/fixed_mask/0020_byte_control",
      test_fixed_mask_0020 },
    { "quant/palette/fixed_mask/0021_forced_float",
      test_fixed_mask_0021 },
    { "quant/palette/fixed_mask/0022_map_srgb",
      test_fixed_mask_0022 },
    { "quant/palette/fixed_mask/0023_map_p3",
      test_fixed_mask_0023 },
    { "quant/palette/fixed_mask/0024_mono",
      test_fixed_mask_0024 },
    { "quant/palette/fixed_mask/0025_inverted_mono",
      test_fixed_mask_0025 },
    { "quant/palette/fixed_mask/0026_wide_image_map",
      test_fixed_mask_0026 },
    { "quant/palette/fixed_mask/0027_byte_wide_map",
      test_fixed_mask_0027 },
    { "quant/palette/fixed_mask/0028_short_mask_retry",
      test_fixed_mask_0028 },
    { "quant/palette/fixed_mask/0029_ormode_retry",
      test_fixed_mask_0029 },
    { "quant/palette/fixed_mask/0030_full256_parallel",
      test_fixed_mask_0030 },
    { "quant/palette/animation_native/0001_gamma_values",
      test_anim_native_0001 },
    { "quant/palette/animation_native/0002_linear_values",
      test_anim_native_0002 },
    { "quant/palette/animation_native/0003_oklab_values",
      test_anim_native_0003 },
    { "quant/palette/animation_native/0004_cielab_values",
      test_anim_native_0004 },
    { "quant/palette/animation_native/0005_din99d_values",
      test_anim_native_0005 },
    { "quant/palette/animation_native/0006_p3_values",
      test_anim_native_0006 },
    { "quant/palette/animation_native/0007_mask_presence",
      test_anim_native_0007 },
    { "quant/palette/animation_native/0008_hidden_values",
      test_anim_native_0008 },
    { "quant/palette/animation_native/0009_empty_coverage",
      test_anim_native_0009 },
    { "quant/palette/animation_native/0010_packed_interpretation",
      test_anim_native_0010 },
    { "quant/palette/animation_native/0011_packed_byte_fast",
      test_anim_native_0011 },
    { "quant/palette/animation_native/0012_packed_p3",
      test_anim_native_0012 },
    { "quant/palette/animation_native/0013_fs_feedback",
      test_anim_native_0013 },
    { "quant/palette/animation_native/0014_fixed_cache_transition",
      test_anim_native_0014 },
    { "quant/palette/animation_native/0015_fs_soft_feedback",
      test_anim_native_0015 },
    { "quant/palette/animation_native/0016_packed_handoff_plan",
      test_anim_native_0016 },
    { "quant/palette/animation_native/0017_rgb_handoff_plan",
      test_anim_native_0017 },
    { "quant/palette/fixed_animation/0001_builtin_gamma", test_fixed_animation_0001 },
    { "quant/palette/fixed_animation/0002_builtin_cielab", test_fixed_animation_0002 },
    { "quant/palette/fixed_animation/0003_builtin_p3", test_fixed_animation_0003 },
    { "quant/palette/fixed_animation/0004_builtin_precision", test_fixed_animation_0004 },
    { "quant/palette/fixed_animation/0005_builtin_output", test_fixed_animation_0005 },
    { "quant/palette/fixed_animation/0006_builtin_workspace", test_fixed_animation_0006 },
    { "quant/palette/fixed_animation/0007_builtin_selection", test_fixed_animation_0007 },
    { "quant/palette/fixed_animation/0008_builtin_fs", test_fixed_animation_0008 },
    { "quant/palette/fixed_animation/0009_mono_oklab", test_fixed_animation_0009 },
    { "quant/palette/fixed_animation/0010_mono_p3", test_fixed_animation_0010 },
    { "quant/palette/fixed_animation/0011_mono_invert", test_fixed_animation_0011 },
    { "quant/palette/fixed_animation/0012_mono_precision", test_fixed_animation_0012 },
    { "quant/palette/fixed_animation/0013_stdin_native", test_fixed_animation_0013 },
    { "quant/palette/fixed_animation/0014_stdin_precision", test_fixed_animation_0014 },
    { "quant/palette/fixed_animation/0015_map_workspace", test_fixed_animation_0015 },
    { "quant/palette/fixed_animation/0016_map_output", test_fixed_animation_0016 },
    { "quant/palette/fixed_animation/0017_image_precision", test_fixed_animation_0017 },
    { "quant/palette/fixed_animation/0018_image_workspace", test_fixed_animation_0018 },
    { "quant/palette/fixed_animation/0019_image_p3", test_fixed_animation_0019 },
    { "quant/palette/fixed_animation/0020_mask_full_table", test_fixed_animation_0020 },
    { "quant/palette/fixed_animation/0021_mask_presence", test_fixed_animation_0021 },
    { "quant/palette/fixed_animation/0022_mask_mono", test_fixed_animation_0022 },
    { "quant/palette/fixed_animation/0023_mask_map", test_fixed_animation_0023 },
    { "quant/palette/fixed_animation/0024_map_precision_request", test_fixed_animation_0024 },
    { "quant/palette/fixed_animation/0025_libpng_pipeline",
      test_fixed_animation_0025 },
    { "quant/palette/fixed_animation/0026_prepared_lookup_reuse",
      test_fixed_animation_0026 },
    { "quant/palette/auto_cover_float/0001_byte_corners_boundary",
      test_auto_cover_0001 },
    { "quant/palette/auto_cover_float/0002_byte_faces_boundary",
      test_auto_cover_0002 },
    { "quant/palette/auto_cover_float/0003_byte_edges_boundary",
      test_auto_cover_0003 },
    { "quant/palette/auto_cover_float/0004_reserved_capacity",
      test_auto_cover_0004 },
    { "quant/palette/auto_cover_float/0005_hard_grow_noop",
      test_auto_cover_0005 },
    { "quant/palette/auto_cover_float/0006_linear_noop",
      test_auto_cover_0006 },
    { "quant/palette/auto_cover_float/0007_explicit_transition",
      test_auto_cover_0007 },
    { "quant/palette/auto_cover_float/0008_encoder_soft_wire",
      test_auto_cover_0008 },
    { "quant/palette/auto_cover_float/0009_encoder_hard_wire",
      test_auto_cover_0009 },
    { "quant/palette/auto_cover_float/0010_default_native",
      test_auto_cover_0010 },
    { "quant/palette/auto_cover_float/0011_explicit_auto_native",
      test_auto_cover_0011 },
    { "quant/palette/auto_cover_float/0012_byte_auto_eligible",
      test_auto_cover_0012 },
    { "quant/palette/auto_cover_float/0013_byte_forced_float",
      test_auto_cover_0013 },
    { "quant/palette/auto_cover_float/0014_float_byte_preference",
      test_auto_cover_0014 },
    { "quant/palette/auto_cover_float/0015_linear_required_float",
      test_auto_cover_0015 },
    { "quant/palette/auto_cover_float/0016_environment_auto",
      test_auto_cover_0016 },
    { "quant/palette/auto_cover_float/0017_environment_hard_grow",
      test_auto_cover_0017 },
    { "quant/palette/auto_cover_float/0018_explicit_auto_precedence",
      test_auto_cover_0018 },
    { "quant/palette/auto_cover_float/0019_p3_default_byte",
      test_auto_cover_0019 },
    { "quant/palette/auto_cover_float/0020_p3_snap_transition",
      test_auto_cover_0020 },
    { "quant/palette/auto_cover_float/0021_encoder_isolation",
      test_auto_cover_0021 },
    { "quant/palette/auto_cover_float/0022_masked_default",
      test_auto_cover_0022 },
    { "quant/palette/auto_cover_float/0023_animation_precision",
      test_auto_cover_0023 },
    { "quant/palette/auto_cover_float/0024_masked_p3_default",
      test_auto_cover_0024 },
    { "quant/palette/auto_cover_float/0025_masked_byte_default",
      test_auto_cover_0025 },
    { "quant/palette/animation_native/0018_libpng_serial",
      test_anim_native_0018 },
    { "quant/palette/animation_native/0019_libpng_pipeline_p3",
      test_anim_native_0019 },
    { "quant/palette/animation_native/0020_builtin_pipeline",
      test_anim_native_0020 },
    { "quant/palette/animation_native/0021_geometry_change",
      test_anim_native_0021 },
    { "quant/palette/packed_alpha/0001_layout_rgba",
      test_packed_0001 },
    { "quant/palette/packed_alpha/0002_layout_argb",
      test_packed_0002 },
    { "quant/palette/packed_alpha/0003_layout_bgra",
      test_packed_0003 },
    { "quant/palette/packed_alpha/0004_layout_abgr",
      test_packed_0004 },
    { "quant/palette/packed_alpha/0005_layout_ga",
      test_packed_0005 },
    { "quant/palette/packed_alpha/0006_layout_ag",
      test_packed_0006 },
    { "quant/palette/packed_alpha/0007_black_background",
      test_packed_0007 },
    { "quant/palette/packed_alpha/0008_alpha_disabled",
      test_packed_0008 },
    { "quant/palette/packed_alpha/0009_binary_byte_fast",
      test_packed_0009 },
    { "quant/palette/packed_alpha/0010_forced_binary_float",
      test_packed_0010 },
    { "quant/palette/packed_alpha/0011_mask_union",
      test_packed_0011 },
    { "quant/palette/packed_alpha/0012_short_mask_atomic",
      test_packed_0012 },
    { "quant/palette/packed_alpha/0013_pixel_allocation_retry",
      test_packed_0013 },
    { "quant/palette/packed_alpha/0014_mask_allocation_retry",
      test_packed_0014 },
    { "quant/palette/packed_alpha/0015_wire_gamma",
      test_packed_0015 },
    { "quant/palette/packed_alpha/0016_wire_linear",
      test_packed_0016 },
    { "quant/palette/packed_alpha/0017_wire_oklab",
      test_packed_0017 },
    { "quant/palette/packed_alpha/0018_wire_cielab",
      test_packed_0018 },
    { "quant/palette/packed_alpha/0019_wire_din99d",
      test_packed_0019 },
    { "quant/palette/packed_alpha/0020_wire_p3",
      test_packed_0020 },
    { "quant/palette/packed_alpha/0021_wire_background",
      test_packed_0021 },
    { "quant/palette/packed_alpha/0022_wire_clear",
      test_packed_0022 },
    { "quant/palette/packed_alpha/0023_wire_byte_fast",
      test_packed_0023 },
    { "quant/palette/packed_alpha/0024_wire_hard_cover",
      test_packed_0024 },
    { "quant/palette/packed_alpha/0025_wire_fixed_palette",
      test_packed_0025 },
    { "quant/palette/packed_alpha/0026_wire_crop",
      test_packed_0026 },
    { "quant/palette/packed_alpha/0027_wire_resize",
      test_packed_0027 },
    { "quant/palette/packed_alpha/0028_wire_encoder_reuse",
      test_packed_0028 },
    { "quant/palette/packed_alpha/0029_wire_all_hidden",
      test_packed_0029 },
    { "quant/palette/packed_alpha/0030_wire_soft_hidden",
      test_packed_0030 },
    { "quant/palette/packed_alpha/0031_wire_alpha_disabled",
      test_packed_0031 },
    { "quant/palette/packed_alpha/0032_wire_fractional_byte_request",
      test_packed_0032 },
    { "quant/palette/packed_alpha/0033_wire_fs_feedback",
      test_packed_0033 },
    { "quant/palette/packed_alpha/0034_wire_allocation_retry",
      test_packed_0034 },
    { "quant/palette/packed_alpha/0035_disabled_alpha_side_mask",
      test_packed_0035 },
    { "quant/palette/alpha_float/0001_srgb_gamma",
      test_alpha_native_0001 },
    { "quant/palette/alpha_float/0002_srgb_linear",
      test_alpha_native_0002 },
    { "quant/palette/alpha_float/0003_srgb_oklab",
      test_alpha_native_0003 },
    { "quant/palette/alpha_float/0004_srgb_cielab",
      test_alpha_native_0004 },
    { "quant/palette/alpha_float/0005_srgb_din99d",
      test_alpha_native_0005 },
    { "quant/palette/alpha_float/0006_p3_gamma",
      test_alpha_native_0006 },
    { "quant/palette/alpha_float/0007_p3_linear",
      test_alpha_native_0007 },
    { "quant/palette/alpha_float/0008_p3_oklab",
      test_alpha_native_0008 },
    { "quant/palette/alpha_float/0009_p3_cielab",
      test_alpha_native_0009 },
    { "quant/palette/alpha_float/0010_p3_din99d",
      test_alpha_native_0010 },
    { "quant/palette/alpha_float/0011_kcenter",
      test_alpha_native_0011 },
    { "quant/palette/alpha_float/0012_kmedoids",
      test_alpha_native_0012 },
    { "quant/palette/alpha_float/0013_heckbert",
      test_alpha_native_0013 },
    { "quant/palette/alpha_float/0014_all_transparent",
      test_alpha_native_0014 },
    { "quant/palette/alpha_float/0015_single_visible",
      test_alpha_native_0015 },
    { "quant/palette/alpha_float/0016_byte_eligible",
      test_alpha_native_0016 },
    { "quant/palette/alpha_float/0017_forced_float",
      test_alpha_native_0017 },
    { "quant/palette/alpha_float/0018_float_over_byte_request",
      test_alpha_native_0018 },
    { "quant/palette/alpha_float/0019_clear_mask",
      test_alpha_native_0019 },
    { "quant/palette/alpha_float/0020_mask_reuse",
      test_alpha_native_0020 },
    { "quant/palette/alpha_float/0021_hidden_invariance",
      test_alpha_native_0021 },
    { "quant/palette/alpha_float/0022_fs_feedback",
      test_alpha_native_0022 },
    { "quant/palette/alpha_float/0023_fhedt_mask",
      test_alpha_native_0023 },
    { "quant/palette/alpha_float/0024_fhedt_reserved",
      test_alpha_native_0024 },
    { "quant/palette/alpha_float/0025_background_key",
      test_alpha_native_0025 },
    { "quant/palette/alpha_float/0026_zero_mask",
      test_alpha_native_0026 },
    { "quant/palette/alpha_float/0027_exact_binning",
      test_alpha_native_0027 },
    { "quant/palette/alpha_float/0028_hard_binning",
      test_alpha_native_0028 },
    { "quant/palette/alpha_float/0029_soft_binning",
      test_alpha_native_0029 },
    { "quant/palette/alpha_float/0030_vptree_reserved",
      test_alpha_native_0030 },
    { "quant/palette/alpha_float/0031_eytzinger_reserved",
      test_alpha_native_0031 },
    { "quant/palette/alpha_float/0032_scan_reserved",
      test_alpha_native_0032 },
    { "quant/palette/alpha_float/0033_cross_space",
      test_alpha_native_0033 },
    { "quant/palette/alpha_float/0034_allocation_retry",
      test_alpha_native_0034 },
    { "quant/palette/mono_float/0001_gamma",
      test_mono_native_0001 },
    { "quant/palette/mono_float/0002_linear",
      test_mono_native_0002 },
    { "quant/palette/mono_float/0003_oklab",
      test_mono_native_0003 },
    { "quant/palette/mono_float/0004_cielab",
      test_mono_native_0004 },
    { "quant/palette/mono_float/0005_din99d",
      test_mono_native_0005 },
    { "quant/palette/mono_float/0006_inverted_gamma",
      test_mono_native_0006 },
    { "quant/palette/mono_float/0007_inverted_linear",
      test_mono_native_0007 },
    { "quant/palette/mono_float/0008_inverted_oklab",
      test_mono_native_0008 },
    { "quant/palette/mono_float/0009_inverted_cielab",
      test_mono_native_0009 },
    { "quant/palette/mono_float/0010_inverted_din99d",
      test_mono_native_0010 },
    { "quant/palette/mono_float/0011_fs_feedback",
      test_mono_native_0011 },
    { "quant/palette/mono_float/0012_inverted_fs",
      test_mono_native_0012 },
    { "quant/palette/mono_float/0013_positive_fs",
      test_mono_native_0013 },
    { "quant/palette/mono_float/0014_negative_fs",
      test_mono_native_0014 },
    { "quant/palette/mono_float/0015_p3_mask",
      test_mono_native_0015 },
    { "quant/palette/mono_float/0016_linear_output",
      test_mono_native_0016 },
    { "quant/palette/mono_float/0017_smptec_output",
      test_mono_native_0017 },
    { "quant/palette/mono_float/0018_byte_eligible",
      test_mono_native_0018 },
    { "quant/palette/mono_float/0019_explicit_float",
      test_mono_native_0019 },
    { "quant/palette/mono_float/0020_precision_reuse",
      test_mono_native_0020 },
    { "quant/palette/mono_float/0021_output_reuse",
      test_mono_native_0021 },
    { "quant/palette/mono_float/0022_inversion_reuse",
      test_mono_native_0022 },
    { "quant/palette/mono_float/0023_clear_mask",
      test_mono_native_0023 },
    { "quant/palette/mono_float/0024_p3_zero_snap",
      test_mono_native_0024 },
    { "quant/palette/mono_float/0025_p3_snap_bypass",
      test_mono_native_0025 },
    { "quant/palette/mono_float/0026_multiframe_native",
      test_mono_native_0026 },
    { "quant/palette/mono_float/0027_cielab_export",
      test_mono_native_0027 },
    { "quant/palette/mono_float/0028_auto_gamma",
      test_mono_native_0028 },
    { "quant/palette/mono_float/0029_auto_cielab",
      test_mono_native_0029 },
    { "quant/palette/mono_float/0030_empty_mask",
      test_mono_native_0030 },
    { "quant/palette/mono_float/0031_full_mask",
      test_mono_native_0031 },
    { "quant/palette/builtin_float/0001_srgb_gamma",
      test_builtin_native_0001 },
    { "quant/palette/builtin_float/0002_srgb_linear",
      test_builtin_native_0002 },
    { "quant/palette/builtin_float/0003_srgb_oklab",
      test_builtin_native_0003 },
    { "quant/palette/builtin_float/0004_srgb_cielab",
      test_builtin_native_0004 },
    { "quant/palette/builtin_float/0005_srgb_din99d",
      test_builtin_native_0005 },
    { "quant/palette/builtin_float/0006_p3_gamma",
      test_builtin_native_0006 },
    { "quant/palette/builtin_float/0007_p3_linear",
      test_builtin_native_0007 },
    { "quant/palette/builtin_float/0008_p3_oklab",
      test_builtin_native_0008 },
    { "quant/palette/builtin_float/0009_p3_cielab",
      test_builtin_native_0009 },
    { "quant/palette/builtin_float/0010_p3_din99d",
      test_builtin_native_0010 },
    { "quant/palette/builtin_float/0011_linear_output",
      test_builtin_native_0011 },
    { "quant/palette/builtin_float/0012_smptec_output",
      test_builtin_native_0012 },
    { "quant/palette/builtin_float/0013_gray2_fs",
      test_builtin_native_0013 },
    { "quant/palette/builtin_float/0014_gray2_p3_fs",
      test_builtin_native_0014 },
    { "quant/palette/builtin_float/0015_vt340_duplicates",
      test_builtin_native_0015 },
    { "quant/palette/builtin_float/0016_byte_eligible",
      test_builtin_native_0016 },
    { "quant/palette/builtin_float/0017_explicit_float",
      test_builtin_native_0017 },
    { "quant/palette/builtin_float/0018_zero_snap",
      test_builtin_native_0018 },
    { "quant/palette/builtin_float/0019_precision_reuse",
      test_builtin_native_0019 },
    { "quant/palette/builtin_float/0020_output_reuse",
      test_builtin_native_0020 },
    { "quant/palette/builtin_float/0021_table_reuse",
      test_builtin_native_0021 },
    { "quant/palette/builtin_float/0022_p3_snap_bypass",
      test_builtin_native_0022 },
    { "quant/palette/builtin_float/0023_multiframe_native",
      test_builtin_native_0023 },
    { "quant/palette/builtin_float/0024_p3_export",
      test_builtin_native_0024 },
    { "quant/palette/builtin_float/0025_positive_fs",
      test_builtin_native_0025 },
    { "quant/palette/builtin_float/0026_negative_fs",
      test_builtin_native_0026 },
    { "quant/palette/output_float/0001_linear_gamma",
      test_output_native_0001 },
    { "quant/palette/output_float/0002_linear_linear",
      test_output_native_0002 },
    { "quant/palette/output_float/0003_linear_oklab",
      test_output_native_0003 },
    { "quant/palette/output_float/0004_linear_cielab",
      test_output_native_0004 },
    { "quant/palette/output_float/0005_linear_din99d",
      test_output_native_0005 },
    { "quant/palette/output_float/0006_smptec_gamma",
      test_output_native_0006 },
    { "quant/palette/output_float/0007_smptec_linear",
      test_output_native_0007 },
    { "quant/palette/output_float/0008_smptec_oklab",
      test_output_native_0008 },
    { "quant/palette/output_float/0009_smptec_cielab",
      test_output_native_0009 },
    { "quant/palette/output_float/0010_smptec_din99d",
      test_output_native_0010 },
    { "quant/palette/output_float/0011_linear_center",
      test_output_native_0011 },
    { "quant/palette/output_float/0012_linear_medoids",
      test_output_native_0012 },
    { "quant/palette/output_float/0013_linear_heckbert",
      test_output_native_0013 },
    { "quant/palette/output_float/0014_linear_auto",
      test_output_native_0014 },
    { "quant/palette/output_float/0015_smptec_center",
      test_output_native_0015 },
    { "quant/palette/output_float/0016_smptec_medoids",
      test_output_native_0016 },
    { "quant/palette/output_float/0017_smptec_heckbert",
      test_output_native_0017 },
    { "quant/palette/output_float/0018_smptec_auto",
      test_output_native_0018 },
    { "quant/palette/output_float/0019_linear_byte",
      test_output_native_0019 },
    { "quant/palette/output_float/0020_smptec_byte",
      test_output_native_0020 },
    { "quant/palette/output_float/0021_linear_zero_snap",
      test_output_native_0021 },
    { "quant/palette/output_float/0022_smptec_zero_snap",
      test_output_native_0022 },
    { "quant/palette/output_float/0023_linear_8bit_preference",
      test_output_native_0023 },
    { "quant/palette/output_float/0024_smptec_8bit_preference",
      test_output_native_0024 },
    { "quant/palette/output_float/0025_linear_fixed_map",
      test_output_native_0025 },
    { "quant/palette/output_float/0026_smptec_fixed_map",
      test_output_native_0026 },
    { "quant/palette/output_float/0027_output_reuse",
      test_output_native_0027 },
    { "lookup/fhedt_byte/0001_y_labels",
      test_fh_byte_0001 },
    { "lookup/fhedt_byte/0002_z_labels",
      test_fh_byte_0002 },
    { "lookup/fhedt_byte/0003_grid",
      test_fh_byte_0003 },
    { "lookup/fhedt_byte/0004_weighted_grid",
      test_fh_byte_0004 },
    { "lookup/fhedt_byte/0005_u16_labels",
      test_fh_byte_0005 },
    { "lookup/fhedt_byte/0006_threaded_grid",
      test_fh_byte_0006 },
    { "lookup/fhedt_byte/0007_refine_labels",
      test_fh_byte_0007 },
    { "lookup/fhedt_byte/0008_cache_reprepare",
      test_fh_byte_0008 },
    { "lookup/fhedt_weight/0001_cielab_default",
      test_fh_weight_0001 },
    { "lookup/fhedt_weight/0002_cielab_unweighted",
      test_fh_weight_0002 },
    { "lookup/fhedt_weight/0003_din99d_default",
      test_fh_weight_0003 },
    { "lookup/fhedt_weight/0004_din99d_unweighted",
      test_fh_weight_0004 },
    { "lookup/fhedt_weight/0005_cielab_refine_weighted",
      test_fh_weight_0005 },
    { "lookup/fhedt_weight/0006_cielab_refine_unweighted",
      test_fh_weight_0006 },
    { "lookup/fhedt_weight/0007_din99d_refine_weighted",
      test_fh_weight_0007 },
    { "lookup/fhedt_weight/0008_din99d_refine_unweighted",
      test_fh_weight_0008 },
    { "lookup/fhedt_weight/0009_outside_weighted",
      test_fh_weight_0009 },
    { "lookup/fhedt_weight/0010_outside_unweighted",
      test_fh_weight_0010 },
    { "lookup/fhedt_weight/0011_expanded_weighted",
      test_fh_weight_0011 },
    { "lookup/fhedt_weight/0012_expanded_unweighted",
      test_fh_weight_0012 },
    { "lookup/fhedt_weight/0013_instance_metric_isolation",
      test_fh_weight_0013 },
    { "lookup/fhedt_weight/0014_rgb_weighted",
      test_fh_weight_0014 },
    { "lookup/fhedt_weight/0015_rgb_unweighted",
      test_fh_weight_0015 },
    { "lookup/fhedt_weight/0016_oklab_default",
      test_fh_weight_0016 },
    { "lookup/fhedt_weight/0017_oklab_unweighted",
      test_fh_weight_0017 },
    { "lookup/fhedt_weight/0018_oklab_explicit",
      test_fh_weight_0018 },
    { "lookup/fhedt_weight/0019_oklab_refine",
      test_fh_weight_0019 },
    { "lookup/fhedt_weight/0020_fractional_l_low",
      test_fh_weight_0020 },
    { "lookup/fhedt_weight/0021_fractional_l_high",
      test_fh_weight_0021 },
    { "lookup/fhedt_weight/0022_opponent_a",
      test_fh_weight_0022 },
    { "lookup/fhedt_weight/0023_opponent_b",
      test_fh_weight_0023 },
    { "lookup/fhedt_weight/0024_common_scale_large",
      test_fh_weight_0024 },
    { "lookup/fhedt_weight/0025_common_scale_small",
      test_fh_weight_0025 },
    { "lookup/fhedt_weight/0026_gamma_channel_ratio",
      test_fh_weight_0026 },
    { "lookup/fhedt_weight/0027_linear_channel_ratio",
      test_fh_weight_0027 },

    { "lookup/fhedt/0018_fh_outside_extreme",
      test_fh_lookup_0018 },
    { "lookup/fhedt/0019_fh_outside_tiny",
      test_fh_lookup_0019 },

    { "lookup/fhedt/0001_fh_gamma_grid",
      test_fh_lookup_0001 },
    { "lookup/fhedt/0002_fh_linear_grid",
      test_fh_lookup_0002 },
    { "lookup/fhedt/0003_fh_oklab_grid",
      test_fh_lookup_0003 },
    { "lookup/fhedt/0004_fh_cielab_grid",
      test_fh_lookup_0004 },
    { "lookup/fhedt/0005_fh_din99d_grid",
      test_fh_lookup_0005 },
    { "lookup/fhedt/0006_fh_gamma_expanded_grid",
      test_fh_lookup_0006 },
    { "lookup/fhedt/0007_fh_cielab_expanded_grid",
      test_fh_lookup_0007 },
    { "lookup/fhedt/0008_fh_din99d_expanded_grid",
      test_fh_lookup_0008 },
    { "lookup/fhedt/0009_fh_outside_cache",
      test_fh_lookup_0009 },
    { "lookup/fhedt/0010_fh_outside_dist2",
      test_fh_lookup_0010 },
    { "lookup/fhedt/0011_fh_native_refine",
      test_fh_lookup_0011 },
    { "lookup/fhedt/0012_fh_native_refine_dist2",
      test_fh_lookup_0012 },
    { "lookup/fhedt/0013_fh_seed_collision",
      test_fh_lookup_0013 },
    { "lookup/fhedt/0014_fh_finite_extrema",
      test_fh_lookup_0014 },
    { "lookup/fhedt/0015_fh_signature",
      test_fh_lookup_0015 },
    { "lookup/fhedt/0016_fh_reprepare",
      test_fh_lookup_0016 },
    { "lookup/fhedt/0017_fh_nonfinite",
      test_fh_lookup_0017 },
    { "palette/fhedt_float/0001_fh_gamma_srgb",
      test_fh_palette_0001 },
    { "palette/fhedt_float/0002_fh_gamma_p3",
      test_fh_palette_0002 },
    { "palette/fhedt_float/0003_fh_linear_srgb",
      test_fh_palette_0003 },
    { "palette/fhedt_float/0004_fh_linear_p3",
      test_fh_palette_0004 },
    { "palette/fhedt_float/0005_fh_oklab_srgb",
      test_fh_palette_0005 },
    { "palette/fhedt_float/0006_fh_oklab_p3",
      test_fh_palette_0006 },
    { "palette/fhedt_float/0007_fh_cielab_srgb",
      test_fh_palette_0007 },
    { "palette/fhedt_float/0008_fh_cielab_p3",
      test_fh_palette_0008 },
    { "palette/fhedt_float/0009_fh_din99d_srgb",
      test_fh_palette_0009 },
    { "palette/fhedt_float/0010_fh_din99d_p3",
      test_fh_palette_0010 },
    { "palette/fhedt_float/0011_fh_byte_eligibility",
      test_fh_palette_0011 },
    { "palette/fhedt_float/0012_fh_bands_private",
      test_fh_palette_0012 },
    { "lookup/eytzinger/0001_eyt_gamma_range",
      test_eyt_lookup_0001 },
    { "lookup/eytzinger/0002_eyt_linear_range",
      test_eyt_lookup_0002 },
    { "lookup/eytzinger/0003_eyt_oklab_range",
      test_eyt_lookup_0003 },
    { "lookup/eytzinger/0004_eyt_cielab_range",
      test_eyt_lookup_0004 },
    { "lookup/eytzinger/0005_eyt_din99d_range",
      test_eyt_lookup_0005 },
    { "lookup/eytzinger/0006_eyt_gamma_fraction",
      test_eyt_lookup_0006 },
    { "lookup/eytzinger/0007_eyt_linear_fraction",
      test_eyt_lookup_0007 },
    { "lookup/eytzinger/0008_eyt_oklab_fraction",
      test_eyt_lookup_0008 },
    { "lookup/eytzinger/0009_eyt_cielab_fraction",
      test_eyt_lookup_0009 },
    { "lookup/eytzinger/0010_eyt_din99d_fraction",
      test_eyt_lookup_0010 },
    { "lookup/eytzinger/0011_eyt_cielab_metric",
      test_eyt_lookup_0011 },
    { "lookup/eytzinger/0012_eyt_din99d_metric",
      test_eyt_lookup_0012 },
    { "lookup/eytzinger/0013_eyt_overflow",
      test_eyt_lookup_0013 },
    { "lookup/eytzinger/0014_eyt_underflow",
      test_eyt_lookup_0014 },
    { "lookup/eytzinger/0015_eyt_isolation",
      test_eyt_lookup_0015 },
    { "lookup/eytzinger/0016_eyt_gamma_oracle",
      test_eyt_lookup_0016 },
    { "lookup/eytzinger/0017_eyt_linear_oracle",
      test_eyt_lookup_0017 },
    { "lookup/eytzinger/0018_eyt_oklab_oracle",
      test_eyt_lookup_0018 },
    { "lookup/eytzinger/0019_eyt_cielab_oracle",
      test_eyt_lookup_0019 },
    { "lookup/eytzinger/0020_eyt_din99d_oracle",
      test_eyt_lookup_0020 },
    { "lookup/eytzinger/0021_eyt_gamma_oracle_large",
      test_eyt_lookup_0021 },
    { "lookup/eytzinger/0022_eyt_linear_oracle_large",
      test_eyt_lookup_0022 },
    { "lookup/eytzinger/0023_eyt_oklab_oracle_large",
      test_eyt_lookup_0023 },
    { "lookup/eytzinger/0024_eyt_cielab_oracle_large",
      test_eyt_lookup_0024 },
    { "lookup/eytzinger/0025_eyt_din99d_oracle_large",
      test_eyt_lookup_0025 },
    { "lookup/eytzinger/0026_eyt_gamma_oracle_small",
      test_eyt_lookup_0026 },
    { "lookup/eytzinger/0027_eyt_linear_oracle_small",
      test_eyt_lookup_0027 },
    { "lookup/eytzinger/0028_eyt_oklab_oracle_small",
      test_eyt_lookup_0028 },
    { "lookup/eytzinger/0029_eyt_cielab_oracle_small",
      test_eyt_lookup_0029 },
    { "lookup/eytzinger/0030_eyt_din99d_oracle_small",
      test_eyt_lookup_0030 },
    { "lookup/eytzinger/0031_eyt_lab_bound",
      test_eyt_lookup_0031 },
    { "lookup/eytzinger/0032_eyt_din99d_bound",
      test_eyt_lookup_0032 },
    { "lookup/eytzinger/0033_eyt_projection_overflow",
      test_eyt_lookup_0033 },
    { "lookup/eytzinger/0034_eyt_gamma_mixed",
      test_eyt_lookup_0034 },
    { "lookup/eytzinger/0035_eyt_linear_mixed",
      test_eyt_lookup_0035 },
    { "lookup/eytzinger/0036_eyt_oklab_mixed",
      test_eyt_lookup_0036 },
    { "lookup/eytzinger/0037_eyt_cielab_mixed",
      test_eyt_lookup_0037 },
    { "lookup/eytzinger/0038_eyt_din99d_mixed",
      test_eyt_lookup_0038 },
    { "palette/eytzinger_float/0001_eyt_gamma_srgb",
      test_eyt_palette_0001 },
    { "palette/eytzinger_float/0002_eyt_gamma_p3",
      test_eyt_palette_0002 },
    { "palette/eytzinger_float/0003_eyt_linear_srgb",
      test_eyt_palette_0003 },
    { "palette/eytzinger_float/0004_eyt_linear_p3",
      test_eyt_palette_0004 },
    { "palette/eytzinger_float/0005_eyt_oklab_srgb",
      test_eyt_palette_0005 },
    { "palette/eytzinger_float/0006_eyt_oklab_p3",
      test_eyt_palette_0006 },
    { "palette/eytzinger_float/0007_eyt_cielab_srgb",
      test_eyt_palette_0007 },
    { "palette/eytzinger_float/0008_eyt_cielab_p3",
      test_eyt_palette_0008 },
    { "palette/eytzinger_float/0009_eyt_din99d_srgb",
      test_eyt_palette_0009 },
    { "palette/eytzinger_float/0010_eyt_din99d_p3",
      test_eyt_palette_0010 },
    { "palette/eytzinger_float/0011_eyt_byte_eligibility",
      test_eyt_palette_0011 },
    { "palette/eytzinger_float/0012_eyt_bands_private",
      test_eyt_palette_0012 },
    { "lookup/mahalanobis/0001_mah_gamma_range",
      test_mah_lookup_0001 },
    { "lookup/mahalanobis/0002_mah_linear_range",
      test_mah_lookup_0002 },
    { "lookup/mahalanobis/0003_mah_oklab_range",
      test_mah_lookup_0003 },
    { "lookup/mahalanobis/0004_mah_cielab_range",
      test_mah_lookup_0004 },
    { "lookup/mahalanobis/0005_mah_din99d_range",
      test_mah_lookup_0005 },
    { "lookup/mahalanobis/0006_mah_gamma_fraction",
      test_mah_lookup_0006 },
    { "lookup/mahalanobis/0007_mah_linear_fraction",
      test_mah_lookup_0007 },
    { "lookup/mahalanobis/0008_mah_oklab_fraction",
      test_mah_lookup_0008 },
    { "lookup/mahalanobis/0009_mah_cielab_fraction",
      test_mah_lookup_0009 },
    { "lookup/mahalanobis/0010_mah_din99d_fraction",
      test_mah_lookup_0010 },
    { "lookup/mahalanobis/0011_mah_cielab_metric",
      test_mah_lookup_0011 },
    { "lookup/mahalanobis/0012_mah_din99d_metric",
      test_mah_lookup_0012 },
    { "lookup/mahalanobis/0013_mah_overflow",
      test_mah_lookup_0013 },
    { "lookup/mahalanobis/0014_mah_underflow",
      test_mah_lookup_0014 },
    { "lookup/mahalanobis/0015_mah_isolation",
      test_mah_lookup_0015 },
    { "lookup/mahalanobis/0016_mah_gamma_oracle",
      test_mah_lookup_0016 },
    { "lookup/mahalanobis/0017_mah_linear_oracle",
      test_mah_lookup_0017 },
    { "lookup/mahalanobis/0018_mah_oklab_oracle",
      test_mah_lookup_0018 },
    { "lookup/mahalanobis/0019_mah_cielab_oracle",
      test_mah_lookup_0019 },
    { "lookup/mahalanobis/0020_mah_din99d_oracle",
      test_mah_lookup_0020 },
    { "lookup/mahalanobis/0021_mah_gamma_oracle_large",
      test_mah_lookup_0021 },
    { "lookup/mahalanobis/0022_mah_linear_oracle_large",
      test_mah_lookup_0022 },
    { "lookup/mahalanobis/0023_mah_oklab_oracle_large",
      test_mah_lookup_0023 },
    { "lookup/mahalanobis/0024_mah_cielab_oracle_large",
      test_mah_lookup_0024 },
    { "lookup/mahalanobis/0025_mah_din99d_oracle_large",
      test_mah_lookup_0025 },
    { "lookup/mahalanobis/0026_mah_gamma_oracle_small",
      test_mah_lookup_0026 },
    { "lookup/mahalanobis/0027_mah_linear_oracle_small",
      test_mah_lookup_0027 },
    { "lookup/mahalanobis/0028_mah_oklab_oracle_small",
      test_mah_lookup_0028 },
    { "lookup/mahalanobis/0029_mah_cielab_oracle_small",
      test_mah_lookup_0029 },
    { "lookup/mahalanobis/0030_mah_din99d_oracle_small",
      test_mah_lookup_0030 },
    { "palette/mahalanobis_float/0001_mah_gamma_srgb",
      test_mah_palette_0001 },
    { "palette/mahalanobis_float/0002_mah_gamma_p3",
      test_mah_palette_0002 },
    { "palette/mahalanobis_float/0003_mah_linear_srgb",
      test_mah_palette_0003 },
    { "palette/mahalanobis_float/0004_mah_linear_p3",
      test_mah_palette_0004 },
    { "palette/mahalanobis_float/0005_mah_oklab_srgb",
      test_mah_palette_0005 },
    { "palette/mahalanobis_float/0006_mah_oklab_p3",
      test_mah_palette_0006 },
    { "palette/mahalanobis_float/0007_mah_cielab_srgb",
      test_mah_palette_0007 },
    { "palette/mahalanobis_float/0008_mah_cielab_p3",
      test_mah_palette_0008 },
    { "palette/mahalanobis_float/0009_mah_din99d_srgb",
      test_mah_palette_0009 },
    { "palette/mahalanobis_float/0010_mah_din99d_p3",
      test_mah_palette_0010 },
    { "palette/mahalanobis_float/0011_mah_byte_eligibility",
      test_mah_palette_0011 },
    { "palette/mahalanobis_float/0012_mah_bands_private",
      test_mah_palette_0012 },
    { "palette/lookup_float/0001_lookup_palette", test_lookup_palette },
    { "palette/merge_float/0002_scan_contract",
      test_ward_scan_populations },
    { "palette/snap_float/0001_snap_contract", test_native_snap },
    { "palette/icc_output/0001_icc_output", test_icc_output },
    { "gamut/0020_edge_srgb_reference",
      test_gamut_0020_edge_srgb_reference },
    { "gamut/0021_edge_p3_reference",
      test_gamut_0021_edge_p3_reference },
    { "gamut/0022_edge_adobe_reference",
      test_gamut_0022_edge_adobe_reference },
    { "gamut/0023_edge_dci_reference",
      test_gamut_0023_edge_dci_reference },
    { "gamut/0024_edge_identity",
      test_gamut_0024_edge_identity },
    { "gamut/0025_edge_endpoints",
      test_gamut_0025_edge_endpoints },
    { "gamut/0026_edge_nonfinite_atomic",
      test_gamut_0026_edge_nonfinite_atomic },
    { "gamut/0027_edge_matrix_icc",
      test_gamut_0027_edge_matrix_icc },
    { "gamut/0028_edge_invalid_boundary",
      test_gamut_0028_edge_invalid_boundary },
    { "gamut/0029_edge_bounds",
      test_gamut_0029_edge_bounds },
    { "gamut/0030_edge_full_frame_samples",
      test_gamut_0030_edge_full_frame_samples },
    { "gamut/0031_edge_adaptive_samples",
      test_gamut_0031_edge_adaptive_samples },
    { "gamut/0032_edge_oklab_workspace",
      test_gamut_0032_edge_oklab_workspace },
    { "gamut/0033_edge_highcolor",
      test_gamut_0033_edge_highcolor },
    { "gamut/0034_edge_lut_rejected",
      test_gamut_0034_edge_lut_rejected },
    { "gamut/0035_edge_environment_precedence",
      test_gamut_0035_edge_environment_precedence },
    { "gamut/0036_edge_encoder_isolation",
      test_gamut_0036_edge_encoder_isolation },
    { "gamut/0038_edge_linear_reference",
      test_gamut_0038_edge_linear_reference },
    { "gamut/0039_ray_srgb_reference",
      test_gamut_0039_ray_srgb_reference },
    { "gamut/0040_ray_p3_reference",
      test_gamut_0040_ray_p3_reference },
    { "gamut/0041_ray_adobe_reference",
      test_gamut_0041_ray_adobe_reference },
    { "gamut/0042_ray_dci_reference",
      test_gamut_0042_ray_dci_reference },
    { "gamut/0043_ray_identity",
      test_gamut_0043_ray_identity },
    { "gamut/0044_ray_endpoints",
      test_gamut_0044_ray_endpoints },
    { "gamut/0045_ray_nonfinite_atomic",
      test_gamut_0045_ray_nonfinite_atomic },
    { "gamut/0046_ray_matrix_icc",
      test_gamut_0046_ray_matrix_icc },
    { "gamut/0047_ray_bounds",
      test_gamut_0047_ray_bounds },
    { "gamut/0048_ray_full_frame_samples",
      test_gamut_0048_ray_full_frame_samples },
    { "gamut/0049_ray_adaptive_samples",
      test_gamut_0049_ray_adaptive_samples },
    { "gamut/0050_ray_oklab_workspace",
      test_gamut_0050_ray_oklab_workspace },
    { "gamut/0051_ray_highcolor",
      test_gamut_0051_ray_highcolor },
    { "gamut/0052_ray_lut_rejected",
      test_gamut_0052_ray_lut_rejected },
    { "gamut/0053_ray_environment_precedence",
      test_gamut_0053_ray_environment_precedence },
    { "gamut/0054_ray_encoder_isolation",
      test_gamut_0054_ray_encoder_isolation },
    { "gamut/0056_ray_geometry",
      test_gamut_0056_ray_geometry },
    { "gamut/0057_ray_cache_reset",
      test_gamut_0057_ray_cache_reset },
    { "gamut/0058_ray_linear_reference",
      test_gamut_0058_ray_linear_reference },
    { "gamut/0059_ray_convergence",
      test_gamut_0059_ray_convergence },
    { "gamut/0013_matrix_icc",
      test_gamut_0013_matrix_icc },
    { "gamut/0014_lut_rejected",
      test_gamut_0014_lut_rejected },
    { "gamut/0015_encoder_isolation",
      test_gamut_0015_encoder_isolation },
    { "gamut/0016_environment_precedence",
      test_gamut_0016_environment_precedence },
    { "gamut/0017_palette_continuous",
      test_gamut_0017_palette_continuous },
    { "gamut/0018_policy_cache_reset",
      test_gamut_0018_policy_cache_reset },

    { "gamut/0009_full_frame_samples", test_gamut_0009_full_frame_samples },
    { "gamut/0010_adaptive_samples", test_gamut_0010_adaptive_samples },
    { "gamut/0011_oklab_workspace", test_gamut_0011_oklab_workspace },
    { "gamut/0012_highcolor", test_gamut_0012_highcolor },

    { "gamut/0001_srgb_identity", test_gamut_0001_srgb_identity },
    { "gamut/0002_p3_extended_identity", test_gamut_0002_p3_extended_identity },
    { "gamut/0003_srgb_reference", test_gamut_0003_srgb_reference },
    { "gamut/0004_p3_reference", test_gamut_0004_p3_reference },
    { "gamut/0005_adobe_reference", test_gamut_0005_adobe_reference },
    { "gamut/0006_dci_reference", test_gamut_0006_dci_reference },
    { "gamut/0007_nonfinite_atomic", test_gamut_0007_nonfinite_atomic },
    { "gamut/0008_policy_atomicity", test_gamut_0008_policy_atomicity },

    { "palette/destination_snap/0001_destination", test_destination_snap },
    { "palette/p3_snap/0001_grid", test_p3_snap_grid },
    { "palette/p3_snap/0002_pipeline", test_p3_snap_pipeline },
    { "quant/palette/cover_mask/0001_cover_mask", test_cover_mask },
    { "palette/cover_float/0001_cover_contract",
      test_palette_cover_native },
    { "palette/merge_float/0001_merge_contract",
      test_palette_merge_float },
    { "palette/solvers_float/0001_solver_contract",
      test_palette_solvers_float },
    { "palette/sampling_float/0001_sampling_contract",
      test_palette_sampling_float },
    { "palette/rgb101_diffusion/0001_spatial_contract",
      test_rgb101_spatial },
    { "palette/0065_srgb_gamma_unsnapped_lookup", test_srgb_0065 },
    { "palette/0066_srgb_linear_unsnapped_lookup", test_srgb_0066 },
    { "palette/0067_srgb_gamma_extended_source", test_srgb_0067 },
    { "palette/0068_srgb_linear_extended_source", test_srgb_0068 },
    { "palette/direct_float", test_palette_direct_float },
    { "palette/retention_kcenter_boundary",
      test_palette_retention_kcenter_boundary },
    { "palette/retention_extended_read",
      test_palette_retention_extended_read },
    { "palette/retention_indexed_write",
      test_palette_retention_indexed_write },
    { "palette/snap_hooks/hook_disabled", test_snap_hooks_hook_disabled },
    { "palette/snap_hooks/policy_disabled", test_snap_hooks_policy_disabled },
    { "palette/snap_hooks/stage_excluded", test_snap_hooks_stage_excluded },
    { "palette/snap_hooks/rate_zero", test_snap_hooks_rate_zero },
    { "palette/snap_hooks/early_nearest", test_snap_hooks_early_nearest },
    { "palette/snap_hooks/early_reversible", test_snap_hooks_early_reversible },
    { "palette/snap_hooks/final_nearest", test_snap_hooks_final_nearest },
    { "palette/snap_hooks/final_reversible", test_snap_hooks_final_reversible },
    { "palette/snap_hooks/large_source", test_snap_hooks_large_source },
    { "palette/snap_hooks/double_blend", test_snap_hooks_double_blend },
    { "palette/snap_hooks/final_rollback", test_snap_hooks_final_rollback },
    { "palette/snap_hooks/byte_grid", test_snap_hooks_byte_grid },
    { "palette/snap_hooks/stage_ladder", test_snap_hooks_stage_ladder },
    { "palette/snap_hooks/kmeans_once", test_snap_hooks_kmeans_once },
    { "palette/snap_hooks/kmeans_all", test_snap_hooks_kmeans_all },
    { "dither/interframe/equal_pixels", test_interframe_equal_pixels },
    { "palette/ordinary_float/center_sources", test_ordinary_center_sources },
    { "palette/ordinary_float/medoids_sources", test_ordinary_medoids_sources },
    { "palette/ordinary_float/center_hard", test_ordinary_center_hard },
    { "palette/ordinary_float/center_exact", test_ordinary_center_exact },
    { "palette/ordinary_float/center_ward", test_ordinary_center_ward },
    { "palette/ordinary_float/medoids_ward", test_ordinary_medoids_ward },
    { "palette/ordinary_float/center_snap", test_ordinary_center_snap },
    { "palette/ordinary_float/medoids_snap", test_ordinary_medoids_snap },
    { "palette/ordinary_float/center_fault", test_ordinary_center_fault },
    { "palette/ordinary_float/medoids_fault", test_ordinary_medoids_fault },
    { "palette/ordinary_float/center_nonfinite", test_ordinary_center_nonfinite },
    { "palette/ordinary_float/medoids_nonfinite", test_ordinary_medoids_nonfinite },
    { "palette/ordinary_float/center_hint", test_ordinary_center_hint },
    { "palette/ordinary_float/medoids_hint", test_ordinary_medoids_hint },
    { "palette/ordinary_float/center_byte", test_ordinary_center_byte },
    { "palette/ordinary_float/medoids_byte", test_ordinary_medoids_byte },
    { "palette/ordinary_float/center_reuse", test_ordinary_center_reuse },
    { "palette/ordinary_float/medoids_reuse", test_ordinary_medoids_reuse },
    { "palette/ordinary_float/center_reduce", test_ordinary_center_reduce },
    { "palette/ordinary_float/medoids_reduce", test_ordinary_medoids_reduce },
    { "palette/ordinary_float/kmeans_sources",
      test_ordinary_kmeans_sources },
    { "palette/ordinary_float/kmeans_hard",
      test_ordinary_kmeans_hard },
    { "palette/ordinary_float/kmeans_exact",
      test_ordinary_kmeans_exact },
    { "palette/ordinary_float/kmeans_ward",
      test_ordinary_kmeans_ward },
    { "palette/ordinary_float/kmeans_snap",
      test_ordinary_kmeans_snap },
    { "palette/ordinary_float/kmeans_fault",
      test_ordinary_kmeans_fault },
    { "palette/ordinary_float/kmeans_nonfinite",
      test_ordinary_kmeans_nonfinite },
    { "palette/ordinary_float/kmeans_hint",
      test_ordinary_kmeans_hint },
    { "palette/ordinary_float/kmeans_byte",
      test_ordinary_kmeans_byte },
    { "palette/ordinary_float/kmeans_reuse",
      test_ordinary_kmeans_reuse },
    { "palette/ordinary_float/kmeans_fractional",
      test_ordinary_kmeans_fractional },
    { "palette/ordinary_float/kmeans_polish",
      test_ordinary_kmeans_polish },
    { "palette/ordinary_float/kmeans_empty",
      test_ordinary_kmeans_empty },
    { "palette/ordinary_float/kmeans_feedback",
      test_ordinary_kmeans_feedback },
    { "palette/ordinary_float/kmeans_feedback_snap",
      test_ordinary_kmeans_feedback_snap },
    { "palette/ordinary_float/kmeans_feedback_interval",
      test_ordinary_kmeans_feedback_interval },
    { "palette/ordinary_float/kmeans_unique",
      test_ordinary_kmeans_unique },
    { "palette/ordinary_float/heckbert_sources",
      test_ordinary_heckbert_sources },
    { "palette/ordinary_float/heckbert_ward",
      test_ordinary_heckbert_ward },
    { "palette/ordinary_float/heckbert_snap",
      test_ordinary_heckbert_snap },
    { "palette/ordinary_float/heckbert_fault",
      test_ordinary_heckbert_fault },
    { "palette/ordinary_float/heckbert_nonfinite",
      test_ordinary_heckbert_nonfinite },
    { "palette/ordinary_float/heckbert_hint",
      test_ordinary_heckbert_hint },
    { "palette/ordinary_float/heckbert_byte",
      test_ordinary_heckbert_byte },
    { "palette/ordinary_float/heckbert_reuse",
      test_ordinary_heckbert_reuse },
    { "palette/ordinary_float/heckbert_center",
      test_ordinary_heckbert_center },
    { "palette/ordinary_float/heckbert_average",
      test_ordinary_heckbert_average },
    { "palette/ordinary_float/heckbert_histogram",
      test_ordinary_heckbert_histogram },
    { "palette/ordinary_float/heckbert_polish",
      test_ordinary_heckbert_polish },
    { "palette/ordinary_float/heckbert_once",
      test_ordinary_heckbert_once },
    { "palette/ordinary_float/heckbert_unique",
      test_ordinary_heckbert_unique },
    { "palette/retention_kmeans",
      test_palette_retention_kmeans },
    { "palette/retention_kcenter",
      test_palette_retention_kcenter },
    { "palette/retention_kmedoids",
      test_palette_retention_kmedoids },
    { "palette/retention_typed",
      test_palette_retention_typed },
    { "palette/retention_getter_write",
      test_palette_retention_getter_write },
    { "palette/retention_setter_same",
      test_palette_retention_setter_same },
    { "palette/retention_wire",
      test_palette_retention_wire },
    { "palette/retention_cache",
      test_palette_retention_cache },
    { "palette/retention_byte",
      test_palette_retention_byte },
    { "palette/retention_invalid",
      test_palette_retention_invalid },
    { "palette/retention_mono",
      test_palette_retention_mono },
    { "palette/retention_feedback",
      test_palette_retention_feedback },
    { "palette/retention_heckbert",
      test_palette_retention_heckbert },
    { "palette/0051_p3_palette_contract", test_palette_0051_p3_contract },
    { "palette/0054_p3_encoder_contract", test_palette_0054_p3_encoder },
    { "palette/0055_p3_mapfile", test_palette_0055_p3_mapfile },
    { "palette/0059_p3_precision_contract", test_palette_0059_p3_precision },
    { "palette/0062_p3_fs_error_contract", test_palette_0062_p3_fs_error },
    { "loader/0008_loader_coregraphics_pixelformat",
      test_loader_0008_loader_coregraphics_pixelformat },
    { "loader/0009_loader_wic_pixelformat",
      test_loader_0009_loader_wic_pixelformat },
    { "loader/0010_loader_gdk_pixbuf_pixelformat",
      test_loader_0010_loader_gdk_pixbuf_pixelformat },
    { "loader/0011_loader_gd_pixelformat",
      test_loader_0011_loader_gd_pixelformat },
    { "loader/libpng_contract", test_loader_libpng_contract },
    { "loader/libpng_indexed_precision", test_libpng_indexed_precision },
    { "cg-expanded/values", test_cg_expanded_values },
    { "cg-indexed/0001_p3_8", test_cgi_0001 },
    { "cg-indexed/0002_p3_1", test_cgi_0002 },
    { "cg-indexed/0003_p3_2", test_cgi_0003 },
    { "cg-indexed/0004_p3_4", test_cgi_0004 },
    { "cg-indexed/0005_p3_no_palette", test_cgi_0005 },
    { "cg-indexed/0006_p3_reqcolors", test_cgi_0006 },
    { "cg-indexed/0007_srgb_byte", test_cgi_0007 },
    { "cg-indexed/0008_srgb_float", test_cgi_0008 },
    { "cg-indexed/0009_p3_engine_none", test_cgi_0009 },
    { "cg-indexed/0010_p3_engine_builtin", test_cgi_0010 },
    { "cg-indexed/0011_allocation_retry", test_cgi_0011 },
    { "cicp/0063_builtin_p3_palette_alpha", test_cicp_0063 },
    { "cicp/0064_libpng_p3_palette_alpha", test_cicp_0064 },
    { "cicp/0065_builtin_srgb_palette_alpha_byte", test_cicp_0065 },
    { "cicp/0066_libpng_srgb_palette_alpha_byte", test_cicp_0066 },
    { "gifc/0030_gif_icc_fail_global", test_gifc_0030 },
    { "gifc/0031_gif_icc_fail_local", test_gifc_0031 },
    { "gifc/0032_gif_icc_fail_canvas", test_gifc_0032 },
    { "gifc/0033_gif_icc_over_limit_reject", test_gifc_0033 },
    { "gifc/0034_gif_icc_limit_exact_numeric", test_gifc_0034 },
    { "gifc/0035_gif_icc_local_only_numeric", test_gifc_0035 },
    { "gifc/0036_gif_icc_explicit_no_global_numeric", test_gifc_0036 },
    { "gifc/0037_gif_icc_gamma_replay_numeric", test_gifc_0037 },
    { "bmpc/0044_bmp_legacy_icc_rgb_numeric", test_bmpc_0044 },
    { "bmpc/0045_bmp_legacy_icc_cmyk_numeric", test_bmpc_0045 },
    { "bmpc/0001_bmp_p3_rgb24_numeric", test_bmpc_0001 },
    { "bmpc/0002_bmp_p3_alpha_numeric", test_bmpc_0002 },
    { "bmpc/0003_bmp_p3_background_numeric", test_bmpc_0003 },
    { "bmpc/0004_bmp_gamma_background_numeric", test_bmpc_0004 },
    { "bmpc/0005_bmp_keep_background_numeric", test_bmpc_0005 },
    { "bmpc/0006_bmp_calibrated_common_numeric", test_bmpc_0006 },
    { "bmpc/0007_bmp_calibrated_split_numeric", test_bmpc_0007 },
    { "bmpc/0008_bmp_indexed_p3_numeric", test_bmpc_0008 },
    { "bmpc/0009_bmp_rle8_p3_numeric", test_bmpc_0009 },
    { "bmpc/0010_bmp_rle4_p3_numeric", test_bmpc_0010 },
    { "bmpc/0011_bmp_rgb10_numeric", test_bmpc_0011 },
    { "bmpc/0012_bmp_p3_rgb10_numeric", test_bmpc_0012 },
    { "bmpc/0013_bmp_alpha11_numeric", test_bmpc_0013 },
    { "bmpc/0014_bmp_p3_rgb565_numeric", test_bmpc_0014 },
    { "bmpc/0015_bmp_forced_rgb565_numeric", test_bmpc_0015 },
    { "bmpc/0016_bmp_topdown_rgb10_numeric", test_bmpc_0016 },
    { "bmpc/0017_bmp_none_rgb10_numeric", test_bmpc_0017 },
    { "bmpc/0018_bmp_rgb24_byte_numeric", test_bmpc_0018 },
    { "bmpc/0019_bmp_binary_alpha_byte_numeric", test_bmpc_0019 },
    { "bmpc/0020_bmp_opaque_background_byte_numeric", test_bmpc_0020 },
    { "bmpc/0021_bmp_partial_background_numeric", test_bmpc_0021 },
    { "bmpc/0022_bmp_device_cmyk_byte_numeric", test_bmpc_0022 },
    { "bmpc/0023_bmp_cmyk32_numeric", test_bmpc_0023 },
    { "bmpc/0024_bmp_cmyk_rle8_numeric", test_bmpc_0024 },
    { "bmpc/0025_bmp_cmyk_rle4_numeric", test_bmpc_0025 },
    { "bmpc/0026_bmp_gamma_target_numeric", test_bmpc_0026 },
    { "bmpc/0027_bmp_prefer8bit_numeric", test_bmpc_0027 },
    { "bmpc/0028_bmp_lcms2_numeric", test_bmpc_0028 },
    { "bmpc/0029_bmp_colorsync_numeric", test_bmpc_0029 },
    { "bmpc/0030_bmp_wrong_rgb_domain_byte_numeric", test_bmpc_0030 },
    { "bmpc/0031_bmp_wrong_cmyk_domain_byte_numeric", test_bmpc_0031 },
    { "bmpc/0032_bmp_invalid_profile_reject", test_bmpc_0032 },
    { "bmpc/0033_bmp_transform_fail", test_bmpc_0033 },
    { "bmpc/0034_bmp_float_alloc_fail", test_bmpc_0034 },
    { "bmpc/0035_bmp_mask_alloc_fail", test_bmpc_0035 },
    { "bmpc/0036_bmp_noncontiguous_reject", test_bmpc_0036 },
    { "bmpc/0037_bmp_overlapping_reject", test_bmpc_0037 },
    { "bmpc/0038_bmp_out_of_range_reject", test_bmpc_0038 },
    { "bmpc/0039_bmp_rgb30_numeric", test_bmpc_0039 },
    { "bmpc/0040_bmp_explicit_zero_alpha_numeric", test_bmpc_0040 },
    { "bmpc/0041_bmp_implicit_zero_alpha_numeric", test_bmpc_0041 },
    { "bmpc/0042_bmp_cmyk_lcms2_numeric", test_bmpc_0042 },
    { "bmpc/0043_bmp_cmyk_colorsync_numeric", test_bmpc_0043 },
    { "gifc/0001_gif_icc_global_numeric", test_gifc_0001 },
    { "gifc/0002_gif_icc_local_numeric", test_gifc_0002 },
    { "gifc/0003_gif_icc_srgb_numeric", test_gifc_0003 },
    { "gifc/0004_gif_icc_gamma2_numeric", test_gifc_0004 },
    { "gifc/0005_gif_icc_split_bytes_numeric", test_gifc_0005 },
    { "gifc/0006_gif_icc_late_profile_numeric", test_gifc_0006 },
    { "gifc/0007_gif_icc_untagged_byte_numeric", test_gifc_0007 },
    { "gifc/0008_gif_icc_cms_none_byte_numeric", test_gifc_0008 },
    { "gifc/0009_gif_icc_alpha_numeric", test_gifc_0009 },
    { "gifc/0010_gif_icc_file_background_numeric", test_gifc_0010 },
    { "gifc/0011_gif_icc_explicit_background_numeric", test_gifc_0011 },
    { "gifc/0012_gif_icc_file_first_background_numeric", test_gifc_0012 },
    { "gifc/0013_gif_icc_disposal2_numeric", test_gifc_0013 },
    { "gifc/0014_gif_icc_disposal3_numeric", test_gifc_0014 },
    { "gifc/0015_gif_icc_interlace_numeric", test_gifc_0015 },
    { "gifc/0016_gif_icc_start_positive_numeric", test_gifc_0016 },
    { "gifc/0017_gif_icc_start_negative_numeric", test_gifc_0017 },
    { "gifc/0018_gif_icc_replay_numeric", test_gifc_0018 },
    { "gifc/0019_gif_icc_unknown_app_byte_numeric", test_gifc_0019 },
    { "gifc/0020_gif_icc_invalid_profile", test_gifc_0020 },
    { "gifc/0021_gif_icc_wrong_domain_reject", test_gifc_0021 },
    { "gifc/0022_gif_icc_duplicate_reject", test_gifc_0022 },
    { "gifc/0023_gif_icc_empty_reject", test_gifc_0023 },
    { "gifc/0024_gif_icc_truncated_blocks", test_gifc_0024 },
    { "gifc/0025_gif_icc_missing_trailer_reject", test_gifc_0025 },
    { "gifc/0026_gif_icc_cms_none_duplicate_numeric", test_gifc_0026 },
    { "gifc/0027_gif_icc_lcms2_numeric", test_gifc_0027 },
    { "gifc/0028_gif_icc_colorsync_numeric", test_gifc_0028 },
    { "gifc/0029_gif_icc_gamma_target_numeric", test_gifc_0029 },
    { "cicp/0001_builtin_p3_rgb8", test_cicp_0001 },
    { "cicp/0002_builtin_p3_rgb16", test_cicp_0002 },
    { "cicp/0003_builtin_p3_rgba8", test_cicp_0003 },
    { "cicp/0004_builtin_p3_rgba16", test_cicp_0004 },
    { "cicp/0005_builtin_p3_indexed", test_cicp_0005 },
    { "cicp/0006_builtin_p3_gray8", test_cicp_0006 },
    { "cicp/0007_builtin_p3_gray16", test_cicp_0007 },
    { "cicp/0008_builtin_p3_grayalpha16", test_cicp_0008 },
    { "cicp/0009_builtin_p3_file_background", test_cicp_0009 },
    { "cicp/0010_builtin_p3_explicit_background", test_cicp_0010 },
    { "cicp/0011_builtin_p3_over_icc", test_cicp_0011 },
    { "cicp/0012_builtin_p3_over_all", test_cicp_0012 },
    { "cicp/0013_builtin_srgb_over_icc_byte", test_cicp_0013 },
    { "cicp/0014_builtin_srgb_byte", test_cicp_0014 },
    { "cicp/0015_builtin_srgb_palette_byte", test_cicp_0015 },
    { "cicp/0016_builtin_pq_fallback_icc", test_cicp_0016 },
    { "cicp/0017_builtin_unknown_fallback_gamma", test_cicp_0017 },
    { "cicp/0018_builtin_narrow_fallback", test_cicp_0018 },
    { "cicp/0019_builtin_matrix_fallback", test_cicp_0019 },
    { "cicp/0020_builtin_crc_fallback", test_cicp_0020 },
    { "cicp/0021_builtin_length_fallback", test_cicp_0021 },
    { "cicp/0022_builtin_duplicate_fallback", test_cicp_0022 },
    { "cicp/0023_builtin_after_plte_fallback", test_cicp_0023 },
    { "cicp/0024_builtin_after_idat_fallback", test_cicp_0024 },
    { "cicp/0025_builtin_cms_disabled", test_cicp_0025 },
    { "cicp/0026_builtin_p3_gamma_target", test_cicp_0026 },
    { "cicp/0027_builtin_p3_lcms2", test_cicp_0027 },
    { "cicp/0028_builtin_p3_colorsync", test_cicp_0028 },
    { "cicp/0029_builtin_p3_apng", test_cicp_0029 },
    { "cicp/0030_builtin_p3_alpha16_background", test_cicp_0030 },
    { "cicp/0031_builtin_p3_adam7", test_cicp_0031 },
    { "cicp/0032_libpng_p3_rgb8", test_cicp_0032 },
    { "cicp/0033_libpng_p3_rgb16", test_cicp_0033 },
    { "cicp/0034_libpng_p3_rgba8", test_cicp_0034 },
    { "cicp/0035_libpng_p3_rgba16", test_cicp_0035 },
    { "cicp/0036_libpng_p3_indexed", test_cicp_0036 },
    { "cicp/0037_libpng_p3_gray8", test_cicp_0037 },
    { "cicp/0038_libpng_p3_gray16", test_cicp_0038 },
    { "cicp/0039_libpng_p3_grayalpha16", test_cicp_0039 },
    { "cicp/0040_libpng_p3_file_background", test_cicp_0040 },
    { "cicp/0041_libpng_p3_explicit_background", test_cicp_0041 },
    { "cicp/0042_libpng_p3_over_icc", test_cicp_0042 },
    { "cicp/0043_libpng_p3_over_all", test_cicp_0043 },
    { "cicp/0044_libpng_srgb_over_icc_byte", test_cicp_0044 },
    { "cicp/0045_libpng_srgb_byte", test_cicp_0045 },
    { "cicp/0046_libpng_srgb_palette_byte", test_cicp_0046 },
    { "cicp/0047_libpng_pq_fallback_icc", test_cicp_0047 },
    { "cicp/0048_libpng_unknown_fallback_gamma", test_cicp_0048 },
    { "cicp/0049_libpng_narrow_fallback", test_cicp_0049 },
    { "cicp/0050_libpng_matrix_fallback", test_cicp_0050 },
    { "cicp/0051_libpng_crc_fallback", test_cicp_0051 },
    { "cicp/0052_libpng_length_fallback", test_cicp_0052 },
    { "cicp/0053_libpng_duplicate_fallback", test_cicp_0053 },
    { "cicp/0054_libpng_after_plte_fallback", test_cicp_0054 },
    { "cicp/0055_libpng_after_idat_fallback", test_cicp_0055 },
    { "cicp/0056_libpng_cms_disabled", test_cicp_0056 },
    { "cicp/0057_libpng_p3_gamma_target", test_cicp_0057 },
    { "cicp/0058_libpng_p3_lcms2", test_cicp_0058 },
    { "cicp/0059_libpng_p3_colorsync", test_cicp_0059 },
    { "cicp/0060_libpng_p3_apng", test_cicp_0060 },
    { "cicp/0061_libpng_p3_alpha16_background", test_cicp_0061 },
    { "cicp/0062_libpng_p3_adam7", test_cicp_0062 },
    { "tgac/0001_gamma2_numeric", test_tgac_0001 },
    { "tgac/0002_gamma1_numeric", test_tgac_0002 },
    { "tgac/0003_gamma_fraction_numeric", test_tgac_0003 },
    { "tgac/0004_gamma_rle_numeric", test_tgac_0004 },
    { "tgac/0005_gamma_gray_numeric", test_tgac_0005 },
    { "tgac/0006_gamma_rgb16_numeric", test_tgac_0006 },
    { "tgac/0007_gamma_index8_numeric", test_tgac_0007 },
    { "tgac/0008_gamma_index16_numeric", test_tgac_0008 },
    { "tgac/0009_gamma_hidden_alpha_numeric", test_tgac_0009 },
    { "tgac/0010_gamma_background_numeric", test_tgac_0010 },
    { "tgac/0011_table_rgb_numeric", test_tgac_0011 },
    { "tgac/0012_table_precedence_numeric", test_tgac_0012 },
    { "tgac/0013_table_before_extension_numeric", test_tgac_0013 },
    { "tgac/0014_table_rle_numeric", test_tgac_0014 },
    { "tgac/0015_table_index_alpha_numeric", test_tgac_0015 },
    { "tgac/0016_table_gray_numeric", test_tgac_0016 },
    { "tgac/0017_table_alpha_background_numeric", test_tgac_0017 },
    { "tgac/0018_cms_none_numeric", test_tgac_0018 },
    { "tgac/0019_legacy_byte_numeric", test_tgac_0019 },
    { "tgac/0020_gamma_unused_numeric", test_tgac_0020 },
    { "tgac/0021_footer_only_numeric", test_tgac_0021 },
    { "tgac/0022_zero_gamma_reject", test_tgac_0022 },
    { "tgac/0023_large_gamma_reject", test_tgac_0023 },
    { "tgac/0024_extension_offset_reject", test_tgac_0024 },
    { "tgac/0025_extension_size_reject", test_tgac_0025 },
    { "tgac/0026_table_offset_reject", test_tgac_0026 },
    { "tgac/0027_table_overlap_reject", test_tgac_0027 },
    { "tgac/0028_short_table_reject", test_tgac_0028 },
    { "tgac/0029_short_raw_reject", test_tgac_0029 },
    { "tgac/0030_short_rle_reject", test_tgac_0030 },
    { "tgac/0031_premultiplied_reject", test_tgac_0031 },
    { "tgac/0032_gamma_lcms2_numeric", test_tgac_0032 },
    { "tgac/0033_gamma_colorsync_numeric", test_tgac_0033 },
    { "tgac/0034_table_lcms2_numeric", test_tgac_0034 },
    { "tgac/0035_table_colorsync_numeric", test_tgac_0035 },
    { "tgac/0036_gamma_target_numeric", test_tgac_0036 },
    { "tgac/0037_gamma_gray_alpha_numeric", test_tgac_0037 },
    { "tgac/0038_table_opaque_alpha_numeric", test_tgac_0038 },
    { "bjicc/0001_bj_gray8_builtin_numeric", test_bjicc_0001 },
    { "bjicc/0002_bj_gray8_lcms2_numeric", test_bjicc_0002 },
    { "bjicc/0003_bj_gray8_colorsync_numeric", test_bjicc_0003 },
    { "bjicc/0004_bj_gray12_builtin_numeric", test_bjicc_0004 },
    { "bjicc/0005_bj_gray12_lcms2_numeric", test_bjicc_0005 },
    { "bjicc/0006_bj_gray12_colorsync_numeric", test_bjicc_0006 },
    { "bjicc/0007_bj_gray16_builtin_numeric", test_bjicc_0007 },
    { "bjicc/0008_bj_gray16_lcms2_numeric", test_bjicc_0008 },
    { "bjicc/0009_bj_gray16_colorsync_numeric", test_bjicc_0009 },
    { "bjicc/0010_bj_plain8_builtin_numeric", test_bjicc_0010 },
    { "bjicc/0011_bj_plain8_lcms2_numeric", test_bjicc_0011 },
    { "bjicc/0012_bj_plain8_colorsync_numeric", test_bjicc_0012 },
    { "bjicc/0013_bj_adobe8_builtin_numeric", test_bjicc_0013 },
    { "bjicc/0014_bj_adobe8_lcms2_numeric", test_bjicc_0014 },
    { "bjicc/0015_bj_adobe8_colorsync_numeric", test_bjicc_0015 },
    { "bjicc/0016_bj_ycck8_builtin_numeric", test_bjicc_0016 },
    { "bjicc/0017_bj_ycck8_lcms2_numeric", test_bjicc_0017 },
    { "bjicc/0018_bj_ycck8_colorsync_numeric", test_bjicc_0018 },
    { "bjicc/0019_bj_plain12_builtin_numeric", test_bjicc_0019 },
    { "bjicc/0020_bj_plain12_lcms2_numeric", test_bjicc_0020 },
    { "bjicc/0021_bj_plain12_colorsync_numeric", test_bjicc_0021 },
    { "bjicc/0022_bj_adobe12_builtin_numeric", test_bjicc_0022 },
    { "bjicc/0023_bj_adobe12_lcms2_numeric", test_bjicc_0023 },
    { "bjicc/0024_bj_adobe12_colorsync_numeric", test_bjicc_0024 },
    { "bjicc/0025_bj_ycck12_builtin_numeric", test_bjicc_0025 },
    { "bjicc/0026_bj_ycck12_lcms2_numeric", test_bjicc_0026 },
    { "bjicc/0027_bj_ycck12_colorsync_numeric", test_bjicc_0027 },
    { "bjicc/0028_bj_plain16_builtin_numeric", test_bjicc_0028 },
    { "bjicc/0029_bj_plain16_lcms2_numeric", test_bjicc_0029 },
    { "bjicc/0030_bj_plain16_colorsync_numeric", test_bjicc_0030 },
    { "bjicc/0031_bj_adobe16_builtin_numeric", test_bjicc_0031 },
    { "bjicc/0032_bj_adobe16_lcms2_numeric", test_bjicc_0032 },
    { "bjicc/0033_bj_adobe16_colorsync_numeric", test_bjicc_0033 },
    { "bjicc/0034_bj_ycck16_builtin_numeric", test_bjicc_0034 },
    { "bjicc/0035_bj_ycck16_lcms2_numeric", test_bjicc_0035 },
    { "bjicc/0036_bj_ycck16_colorsync_numeric", test_bjicc_0036 },
    { "bjicc/0037_bj_progressive8_builtin_numeric", test_bjicc_0037 },
    { "bjicc/0038_bj_progressive8_lcms2_numeric", test_bjicc_0038 },
    { "bjicc/0039_bj_progressive8_colorsync_numeric", test_bjicc_0039 },
    { "bjicc/0040_bj_ink_none_byte_numeric", test_bjicc_0040 },
    { "bjicc/0041_bj_ink_absent_byte_numeric", test_bjicc_0041 },
    { "bjicc/0042_bj_ink_rejected_numeric", test_bjicc_0042 },
    { "bjicc/0043_bj_ink_apply_failure_numeric", test_bjicc_0043 },
    { "bjicc/0044_bj_ink_mismatch_numeric", test_bjicc_0044 },
    { "bjicc/0045_bj_ink_none_float_numeric", test_bjicc_0045 },
    { "bjicc/0046_bj_ink_absent_float_numeric", test_bjicc_0046 },
    { "bjicc/0047_bj_gray_none_byte_numeric", test_bjicc_0047 },
    { "bjicc/0048_bj_gray_absent_byte_numeric", test_bjicc_0048 },
    { "bjicc/0049_bj_gray_rejected_numeric", test_bjicc_0049 },
    { "bjicc/0050_bj_gray_none12_float_numeric", test_bjicc_0050 },
    { "bjicc/0051_bj_gray_none16_float_numeric", test_bjicc_0051 },
    { "bjicc/0052_bj_ink8_allocation", test_bjicc_0052 },
    { "bjicc/0053_bj_ink16_allocation", test_bjicc_0053 },
    { "jpeg-gray/0001_gray8_builtin", test_jgray_0001 },
    { "jpeg-gray/0002_gray8_lcms2", test_jgray_0002 },
    { "jpeg-gray/0003_gray8_colorsync", test_jgray_0003 },
    { "jpeg-gray/0004_gray12_builtin", test_jgray_0004 },
    { "jpeg-gray/0005_gray12_lcms2", test_jgray_0005 },
    { "jpeg-gray/0006_gray12_colorsync", test_jgray_0006 },
    { "jpeg-gray/0007_gray16_builtin", test_jgray_0007 },
    { "jpeg-gray/0008_gray16_lcms2", test_jgray_0008 },
    { "jpeg-gray/0009_gray16_colorsync", test_jgray_0009 },
    { "jpeg-gray/0010_cms_none_byte", test_jgray_0010 },
    { "jpeg-gray/0011_untagged_byte", test_jgray_0011 },
    { "jpeg-gray/0012_rejected_profile", test_jgray_0012 },
    { "jpeg-gray/0013_gray12_cms_none", test_jgray_0013 },
    { "jpeg-gray/0014_untagged_float", test_jgray_0014 },
    { "jpeg-gray/0015_cms_none_float", test_jgray_0015 },
    { "jpeg-gray/0016_gray16_cms_none", test_jgray_0016 },
    { "loader/0012_loader_libpng_pixelformat",
      test_loader_0012_loader_libpng_pixelformat },
    { "loader/0013_loader_libjpeg_pixelformat",
      test_loader_0013_loader_libjpeg_pixelformat },
    { "loader/0014_loader_builtin_pixelformat",
      test_loader_0014_loader_builtin_pixelformat },
    { "loader/0015_loader_quicklook_pixelformat",
      test_loader_0015_loader_quicklook_pixelformat },
    { "loader/0016_loader_gnome_thumbnailer_pixelformat",
      test_loader_0016_loader_gnome_thumbnailer_pixelformat },
    { "aborttrace/0001_img2sixel_aborttrace",
      test_aborttrace_0001_img2sixel_aborttrace },
    { "loader/0017_loader_libwebp_pixelformat",
      test_loader_0017_loader_libwebp_pixelformat },
    { "loader/0018_loader_libwebp_animation_frames",
      test_loader_0018_loader_libwebp_animation_frames },
    { "loader/0019_loader_libwebp_palette_promotion_guard",
      test_loader_0019_loader_libwebp_palette_promotion_guard },
    { "loader/0024_loader_libwebp_fault_demux",
      test_loader_0024_loader_libwebp_fault_demux },
    { "loader/0026_loader_libwebp_fault_options_init",
      test_loader_0026_loader_libwebp_fault_options_init },
    { "loader/0027_loader_libwebp_fault_decoder_new",
      test_loader_0027_loader_libwebp_fault_decoder_new },
    { "loader/0028_loader_libwebp_fault_decoder_getinfo",
      test_loader_0028_loader_libwebp_fault_decoder_getinfo },
    { "loader/0029_loader_libwebp_fault_decoder_getnext",
      test_loader_0029_loader_libwebp_fault_decoder_getnext },
    { "loader/0032_loader_libwebp_fault_static_rgbinto",
      test_loader_0032_loader_libwebp_fault_static_rgbinto },
    { "loader/0033_loader_libwebp_fault_static_rgbainto",
      test_loader_0033_loader_libwebp_fault_static_rgbainto },
    { "loader/0034_loader_libwebp_fault_lossy_init_config",
      test_loader_0034_loader_libwebp_fault_lossy_init_config },
    { "loader/0035_loader_libwebp_fault_lossy_decode",
      test_loader_0035_loader_libwebp_fault_lossy_decode },
    { "loader/0036_loader_libwebp_fault_lossy_yuv_plane_missing",
      test_loader_0036_loader_libwebp_fault_lossy_yuv_plane_missing },
    { "loader/0037_loader_libwebp_fault_lossy_yuv_stride_invalid",
      test_loader_0037_loader_libwebp_fault_lossy_yuv_stride_invalid },
    { "loader/0038_loader_libwebp_fault_lossy_dimensions_mismatch",
      test_loader_0038_loader_libwebp_fault_lossy_dimensions_mismatch },
    { "loader/0039_loader_libwebp_fault_no_frames",
      test_loader_0039_loader_libwebp_fault_no_frames },
    { "loader/0040_loader_libwebp_fault_no_anmf",
      test_loader_0040_loader_libwebp_fault_no_anmf },
    { "loader/0041_loader_libwebp_frame_count_limit_fast",
      test_loader_0041_loader_libwebp_frame_count_limit_fast },
    { "loader/0042_loader_libwebp_frame_count_limit_decoder_guard",
      test_loader_0042_loader_libwebp_frame_count_limit_decoder_guard },
    { "loader/0043_loader_libwebp_fault_get_features_static",
      test_loader_0043_loader_libwebp_fault_get_features_static },
    { "loader/0044_loader_libwebp_fault_get_features_animation",
      test_loader_0044_loader_libwebp_fault_get_features_animation },
    { "loader/0045_loader_libwebp_fault_static_malloc",
      test_loader_0045_loader_libwebp_fault_static_malloc },
    { "loader/0046_loader_libwebp_fault_animation_canvas_malloc",
      test_loader_0046_loader_libwebp_fault_animation_canvas_malloc },
    { "loader/0047_loader_libwebp_fault_lossy_malloc",
      test_loader_0047_loader_libwebp_fault_lossy_malloc },
    { "loader/0048_loader_libwebp_fault_anmf_payload_too_small",
      test_loader_0048_loader_libwebp_fault_anmf_payload_too_small },
    { "loader/0049_loader_libwebp_fault_anmf_extract_malloc",
      test_loader_0049_loader_libwebp_fault_anmf_extract_malloc },
    { "loader/0050_loader_osc11_colorspec_parser",
      test_loader_0050_loader_osc11_colorspec_parser },
    { "loader/0051_loader_osc11_response_parser",
      test_loader_0051_loader_osc11_response_parser },
    { "loader/0052_loader_osc11_query_control",
      test_loader_0052_loader_osc11_query_control },
    { "loader/0053_loader_background_colorspace_override",
      test_loader_0053_loader_background_colorspace_override },
    { "loader/0054_loader_gif_bgcolor_canvas_fill",
      test_loader_0054_loader_gif_bgcolor_canvas_fill },
    { "loader/0055_loader_animation_hide_cursor_control",
      test_loader_0055_loader_animation_hide_cursor_control },
    { "loader/0056_loader_factory_predicate_gate",
      test_loader_0056_loader_factory_predicate_gate },
    { "loader/0057_loader_gd_can_try_policy",
      test_loader_0057_loader_gd_can_try_policy },
    { "loader/0058_loader_gd_status_policy",
      test_loader_0058_loader_gd_status_policy },
    { "loader/0059_loader_gd_pixelpolicy_detail",
      test_loader_0059_loader_gd_pixelpolicy_detail },
    { "loader/0061_loader_tty_cpr_parser",
      test_loader_0061_loader_tty_cpr_parser },
    { "loader/0062_loader_animation_hide_cursor_environment",
      test_loader_0062_loader_animation_hide_cursor_environment },
    { "loader/0063_loader_builtin_rgba_alpha_mask",
      test_loader_0063_loader_builtin_rgba_alpha_mask },
    { "loader/0064_loader_tty_response_dispatcher",
      test_loader_0064_loader_tty_response_dispatcher },
    { "loader/0065_loader_osc11_colorspec_reject",
      test_loader_0065_loader_osc11_colorspec_reject },
    { "loader/0066_loader_osc11_response_reject",
      test_loader_0066_loader_osc11_response_reject },
    { "loader/0067_loader_wait_for_condition",
      test_loader_0067_loader_wait_for_condition },
    { "loader/0068_loader_background_colorspace_gamma",
      test_loader_0068_loader_background_colorspace_gamma },
    { "loader/0069_loader_background_colorspace_reset",
      test_loader_0069_loader_background_colorspace_reset },
    { "loader/0070_loader_gif_bgcolor_canvas_fill_explicit_first",
      test_loader_0070_loader_gif_bgcolor_canvas_fill_explicit_first },
    { "loader/0071_loader_builtin_gif87a_numeric",
      test_loader_0071_loader_builtin_gif87a_numeric },
    { "loader/0072_loader_builtin_gif_interlace_four_pass_numeric",
      test_loader_0072_loader_builtin_gif_interlace_four_pass_numeric },
    { "loader/0073_loader_builtin_gif_rectangle_offset_numeric",
      test_loader_0073_loader_builtin_gif_rectangle_offset_numeric },
    { "loader/0074_loader_builtin_gif_rectangle_oob_reject",
      test_loader_0074_loader_builtin_gif_rectangle_oob_reject },
    { "loader/0075_loader_builtin_gif_extension_subblocks_numeric",
      test_loader_0075_loader_builtin_gif_extension_subblocks_numeric },
    { "loader/0076_loader_builtin_gif_truncated_raster_reject",
      test_loader_0076_loader_builtin_gif_truncated_raster_reject },
    { "loader/0077_loader_builtin_gif_illegal_lzw_code_reject",
      test_loader_0077_loader_builtin_gif_illegal_lzw_code_reject },
    { "loader/0078_loader_builtin_gif_disposal2_numeric",
      test_loader_0078_loader_builtin_gif_disposal2_numeric },
    { "loader/0079_loader_builtin_gif_lzw_12bit_width_numeric",
      test_loader_0079_loader_builtin_gif_lzw_12bit_width_numeric },
    { "loader/0080_loader_builtin_tga_truecolor16_numeric",
      test_loader_0080_loader_builtin_tga_truecolor16_numeric },
    { "loader/0081_loader_builtin_tga_palette8_numeric",
      test_loader_0081_loader_builtin_tga_palette8_numeric },
    { "loader/0082_loader_builtin_tga_palette16_numeric",
      test_loader_0082_loader_builtin_tga_palette16_numeric },
    { "loader/0083_loader_builtin_tga_index16_numeric",
      test_loader_0083_loader_builtin_tga_index16_numeric },
    { "loader/0084_loader_builtin_tga_bottom_origin_numeric",
      test_loader_0084_loader_builtin_tga_bottom_origin_numeric },
    { "loader/0085_loader_builtin_tga_rle_packet_modes_numeric",
      test_loader_0085_loader_builtin_tga_rle_packet_modes_numeric },
    { "loader/0086_loader_builtin_tga_truncated_palette_reject",
      test_loader_0086_loader_builtin_tga_truncated_palette_reject },
    { "loader/0087_loader_builtin_tga_oob_index_fallback_numeric",
      test_loader_0087_loader_builtin_tga_oob_index_fallback_numeric },
    { "loader/0088_loader_builtin_pic_omitted_channels_numeric",
      test_loader_0088_loader_builtin_pic_omitted_channels_numeric },
    { "loader/0089_loader_builtin_pic_repeated_channel_overwrite_numeric",
      test_loader_0089_pic_repeated_channel_numeric },
    { "loader/0090_loader_builtin_pic_pure_rle_oversized_clip_numeric",
      test_loader_0090_pic_pure_rle_clip_numeric },
    { "loader/0091_loader_builtin_pic_mixed_rle_raw128_numeric",
      test_loader_0091_loader_builtin_pic_mixed_rle_raw128_numeric },
    { "loader/0092_loader_builtin_pic_mixed_rle_repeat128_numeric",
      test_loader_0092_loader_builtin_pic_mixed_rle_repeat128_numeric },
    { "loader/0093_loader_builtin_pic_ten_packet_limit_numeric",
      test_loader_0093_loader_builtin_pic_ten_packet_limit_numeric },
    { "loader/0094_loader_builtin_png_apng_dispose_previous_numeric",
      test_loader_0094_builtin_png_apng_dispose_previous_numeric },
    { "loader/0095_loader_builtin_jpeg_ycck8_rgb_digest",
      test_loader_0095_loader_builtin_jpeg_ycck8_rgb_digest },
    { "loader/0096_loader_builtin_psd_missing_composite_rgb8_digest",
      test_loader_0096_builtin_psd_missing_composite_rgb8_digest },
    { "loader/0098_loader_builtin_pic_raw_rgb_numeric",
      test_loader_0098_loader_builtin_pic_raw_rgb_numeric },
    { "loader/0099_loader_builtin_pic_mixed_rle_extended_numeric",
      test_loader_0099_loader_builtin_pic_mixed_rle_extended_numeric },
    { "loader/0100_loader_builtin_tga_truecolor24_numeric",
      test_loader_0100_loader_builtin_tga_truecolor24_numeric },
    { "loader/0101_loader_builtin_tga_grayscale8_numeric",
      test_loader_0101_loader_builtin_tga_grayscale8_numeric },
    { "loader/0102_loader_builtin_pic_chained_packets_numeric",
      test_loader_0102_loader_builtin_pic_chained_packets_numeric },
    { "loader/0103_loader_builtin_gif_disposal3_numeric",
      test_loader_0103_loader_builtin_gif_disposal3_numeric },
    { "loader/0104_loader_builtin_gif_local_palette_numeric",
      test_loader_0104_loader_builtin_gif_local_palette_numeric },
    { "loader/0105_loader_builtin_hdr_new_rle_numeric",
      test_loader_0105_loader_builtin_hdr_new_rle_numeric },
    { "loader/0106_loader_builtin_hdr_old_scanline_rle_numeric",
      test_loader_0106_loader_builtin_hdr_old_scanline_rle_numeric },
    { "loader/0107_loader_builtin_hdr_legacy_stream_numeric",
      test_loader_0107_loader_builtin_hdr_legacy_stream_numeric },
    { "loader/0108_loader_builtin_jpeg_rgb8_sequential_digest",
      test_loader_0108_loader_builtin_jpeg_rgb8_sequential_digest },
    { "loader/0109_loader_builtin_png_icc_numeric",
      test_loader_0109_loader_builtin_png_icc_numeric },
    { "loader/0110_loader_builtin_jpeg_icc_numeric",
      test_loader_0110_loader_builtin_jpeg_icc_numeric },
    { "loader/0111_loader_builtin_psd_icc_digest",
      test_loader_0111_loader_builtin_psd_icc_digest },
    { "loader/0115_loader_builtin_jpeg_rgb16_lossless_numeric",
      test_loader_0115_loader_builtin_jpeg_rgb16_lossless_numeric },
    { "loader/0116_loader_builtin_psd_rgb16_zip_pred_numeric",
      test_loader_0116_loader_builtin_psd_rgb16_zip_pred_numeric },
    { "loader/0117_loader_builtin_psd_rgb32_rle_numeric",
      test_loader_0117_loader_builtin_psd_rgb32_rle_numeric },
    { "loader/0118_loader_builtin_sixel_unpainted_numeric",
      test_loader_0118_loader_builtin_sixel_unpainted_numeric },
    { "loader/0119_loader_builtin_sixel_high_color_numeric",
      test_loader_0119_loader_builtin_sixel_high_color_numeric },
    { "loader/0120_loader_builtin_pic_zero_dimension_reject",
      test_loader_0120_loader_builtin_pic_zero_dimension_reject },
    { "loader/0121_loader_builtin_pic_eleventh_packet_reject",
      test_loader_0121_loader_builtin_pic_eleventh_packet_reject },
    { "loader/0122_loader_builtin_tga_truncated_direct_reject",
      test_loader_0122_loader_builtin_tga_truncated_direct_reject },
    { "loader/0123_loader_builtin_tga_rle_overrun_reject",
      test_loader_0123_loader_builtin_tga_rle_overrun_reject },
    { "loader/0124_loader_manager_open_plan",
      test_loader_0124_loader_manager_open_plan },
    { "loader/0125_loader_manager_closed_plan",
      test_loader_0125_loader_manager_closed_plan },
    { "loader/0126_loader_manager_fallback_status",
      test_loader_0126_loader_manager_fallback_status },
    { "loader/0127_loader_builtin_gif_animation_metadata_numeric",
      test_loader_0127_loader_builtin_gif_animation_metadata_numeric },
    { "loader/0128_loader_builtin_apng_animation_metadata_numeric",
      test_loader_0128_loader_builtin_apng_animation_metadata_numeric },
    { "loader/0130_loader_builtin_hdr_new_rle_zero_count_reject",
      test_loader_0130_loader_builtin_hdr_new_rle_zero_count_reject },
    { "loader/0131_loader_builtin_hdr_new_rle_overrun_reject",
      test_loader_0131_loader_builtin_hdr_new_rle_overrun_reject },
    { "loader/0132_loader_builtin_hdr_new_rle_truncated_reject",
      test_loader_0132_loader_builtin_hdr_new_rle_truncated_reject },
    { "loader/0133_loader_builtin_hdr_legacy_repeat_first_reject",
      test_loader_0133_loader_builtin_hdr_legacy_repeat_first_reject },
    { "loader/0134_loader_builtin_hdr_legacy_repeat_overrun_reject",
      test_loader_0134_hdr_legacy_repeat_overrun_reject },
    { "loader/0135_loader_builtin_png_filter_modes_numeric",
      test_loader_0135_loader_builtin_png_filter_modes_numeric },
    { "loader/0136_loader_builtin_png_adam7_indexed4_digest",
      test_loader_0136_loader_builtin_png_adam7_indexed4_digest },
    { "loader/0137_loader_builtin_png_gray16_sub8bit_numeric",
      test_loader_0137_loader_builtin_png_gray16_sub8bit_numeric },
    { "loader/0138_loader_builtin_apng_blend_over_numeric",
      test_loader_0138_loader_builtin_apng_blend_over_numeric },
    { "loader/0139_loader_builtin_apng_dispose_background_numeric",
      test_loader_0139_loader_builtin_apng_dispose_background_numeric },
    { "loader/0140_loader_builtin_jpeg_gray16_sub8bit_numeric",
      test_loader_0140_loader_builtin_jpeg_gray16_sub8bit_numeric },
    { "loader/0141_loader_builtin_jpeg_progressive_420_digest",
      test_loader_0141_loader_builtin_jpeg_progressive_420_digest },
    { "loader/0142_loader_builtin_jpeg_gray16_restart_numeric",
      test_loader_0142_loader_builtin_jpeg_gray16_restart_numeric },
    { "loader/0143_loader_builtin_jpeg_ycbcr444_digest",
      test_loader_0143_loader_builtin_jpeg_ycbcr444_digest },
    { "loader/0144_loader_builtin_jpeg_cmyk8_digest",
      test_loader_0144_loader_builtin_jpeg_cmyk8_digest },
    { "loader/0150_loader_builtin_psd_rgb8_alpha_mask_digest",
      test_loader_0150_loader_builtin_psd_rgb8_alpha_mask_digest },
    { "loader/0151_loader_builtin_psd_rgb8_alpha_white_digest",
      test_loader_0151_loader_builtin_psd_rgb8_alpha_white_digest },
    { "loader/0152_loader_builtin_psd_cmyk16_zip_pred_numeric",
      test_loader_0152_loader_builtin_psd_cmyk16_zip_pred_numeric },
    { "loader/0153_loader_builtin_psd_lab32_zip_pred_numeric",
      test_loader_0153_loader_builtin_psd_lab32_zip_pred_numeric },
    { "loader/0154_loader_builtin_psb_layer_info_u64max_reject",
      test_loader_0154_loader_builtin_psb_layer_info_u64max_reject },
    { "loader/0155_loader_builtin_allocator_failure_matrix",
      test_loader_0155_loader_builtin_allocator_failure_matrix },
    { "loader/0156_loader_builtin_callback_status_matrix",
      test_loader_0156_loader_builtin_callback_status_matrix },
    { "loader/0157_loader_builtin_png_cgbi_numeric",
      test_loader_0157_loader_builtin_png_cgbi_numeric },
    { "loader/0158_loader_builtin_psd_blend_modes_numeric",
      test_loader_0158_loader_builtin_psd_blend_modes_numeric },
    { "loader/0159_loader_builtin_psd_clipping_digest",
      test_loader_0159_loader_builtin_psd_clipping_digest },
    { "loader/0160_loader_builtin_psd_mask_digest",
      test_loader_0160_loader_builtin_psd_mask_digest },
    { "loader/0161_loader_builtin_psd_effect_digest",
      test_loader_0161_loader_builtin_psd_effect_digest },
    { "loader/0162_loader_builtin_apng_cancel_boundaries",
      test_loader_0162_loader_builtin_apng_cancel_boundaries },
    { "loader/0164_loader_builtin_jpeg_lossless_predictors_numeric",
      test_loader_0164_loader_builtin_jpeg_lossless_preds_numeric },
    { "loader/0165_loader_builtin_jpeg_rgb_restart_numeric",
      test_loader_0165_loader_builtin_jpeg_rgb_restart_numeric },
    { "loader/0166_loader_builtin_apng_allocation_failures",
      test_loader_0166_loader_builtin_apng_allocation_failures },
    { "loader/0168_loader_builtin_psd_effect_allocation_failures",
      test_loader_0168_loader_builtin_psd_effect_allocation_failures },
    { "loader/0169_loader_builtin_png_icc_allocation_failures",
      test_loader_0169_loader_builtin_png_icc_allocation_failures },
    { "loader/0173_loader_builtin_tga_header_collision_reject",
      test_loader_0173_loader_builtin_tga_header_collision_reject },
    { "loader/0174_loader_builtin_png_chrm_gama_float_numeric",
      test_loader_0174_loader_builtin_png_chrm_gama_float_numeric },
    { "loader/0175_loader_builtin_png_singular_chrm_fallback_numeric",
      test_loader_0175_loader_builtin_png_singular_chrm_numeric },
    { "loader/0176_loader_builtin_png_file_background_chrm_numeric",
      test_loader_0176_loader_builtin_png_bkgd_chrm_numeric },
    { "loader/0177_loader_builtin_png_post_idat_actl_static_numeric",
      test_loader_0177_png_post_idat_actl_static },
    { "loader/0178_loader_builtin_apng_default_image_excluded_numeric",
      test_loader_0178_apng_default_excluded },
    { "loader/0179_loader_builtin_apng_early_error_static_fallback_numeric",
      test_loader_0179_apng_early_static_fallback },
    { "loader/0180_loader_builtin_apng_late_frame_count_reject",
      test_loader_0180_apng_late_count_reject },
    { "loader/0181_loader_builtin_apng_late_sequence_reject",
      test_loader_0181_apng_late_sequence_reject },
    { "loader/0182_loader_builtin_apng_callback_error_no_fallback",
      test_loader_0182_apng_callback_no_fallback },
    { "loader/0183_loader_builtin_jpeg_source_numeric",
      test_loader_0183_jpeg_source_numeric },
    { "loader/0184_loader_builtin_psd_cmyk_icc_numeric",
      test_loader_0184_psd_cmyk_icc_numeric },
    { "loader/psd/0001_duotone_raw_numeric", test_psd_color_0001 },
    { "loader/psd/0002_duotone_rle_numeric", test_psd_color_0002 },
    { "loader/psd/0003_duotone_zip_numeric", test_psd_color_0003 },
    { "loader/psd/0004_duotone_prediction_numeric", test_psd_color_0004 },
    { "loader/psd/0005_duotone_psb_rle_numeric", test_psd_color_0005 },
    { "loader/psd/0006_duotone_16_numeric", test_psd_color_0006 },
    { "loader/psd/0007_duotone_32_numeric", test_psd_color_0007 },
    { "loader/psd/0008_duotone_gamut_numeric", test_psd_color_0008 },
    { "loader/psd/0009_duotone_alpha_numeric", test_psd_color_0009 },
    { "loader/psd/0010_duotone_background_numeric", test_psd_color_0010 },
    { "loader/psd/0011_duotone_builtin_numeric", test_psd_color_0011 },
    { "loader/psd/0012_spot_one_numeric", test_psd_color_0012 },
    { "loader/psd/0013_spot_two_numeric", test_psd_color_0013 },
    { "loader/psd/0014_spot_three_numeric", test_psd_color_0014 },
    { "loader/psd/0015_spot_four_numeric", test_psd_color_0015 },
    { "loader/psd/0016_spot_five_numeric", test_psd_color_0016 },
    { "loader/psd/0017_spot_max_numeric", test_psd_color_0017 },
    { "loader/psd/0018_spot_16_numeric", test_psd_color_0018 },
    { "loader/psd/0019_spot_32_numeric", test_psd_color_0019 },
    { "loader/psd/0020_spot_rle_numeric", test_psd_color_0020 },
    { "loader/psd/0021_spot_zip_numeric", test_psd_color_0021 },
    { "loader/psd/0022_spot_prediction_numeric", test_psd_color_0022 },
    { "loader/psd/0023_spot_psb_numeric", test_psd_color_0023 },
    { "loader/psd/0024_spot_obsolete_numeric", test_psd_color_0024 },
    { "loader/psd/0025_spot_solidity_numeric", test_psd_color_0025 },
    { "loader/psd/0026_duotone_short_reject", test_psd_color_0026 },
    { "loader/psd/0027_duotone_count_reject", test_psd_color_0027 },
    { "loader/psd/0028_duotone_version_reject", test_psd_color_0028 },
    { "loader/psd/0029_display_short_reject", test_psd_color_0029 },
    { "loader/psd/0030_display_opacity_reject", test_psd_color_0030 },
    { "loader/psd/0031_display_kind_reject", test_psd_color_0031 },
    { "loader/psd/0032_display_count_reject", test_psd_color_0032 },
    { "loader/psd/0033_spot_space_reject", test_psd_color_0033 },
    { "loader/psd/0034_spot_nonfinite_reject", test_psd_color_0034 },
    { "loader/psd/0035_spot_range_reject", test_psd_color_0035 },
    { "loader/psd/0036_spot_plane_short_reject", test_psd_color_0036 },
    { "loader/psd/0037_display_precedence_numeric", test_psd_color_0037 },
    { "loader/psd/0038_resource_padding_reject", test_psd_color_0038 },
    { "loader/psd/0039_duotone_duplicate_reject", test_psd_color_0039 },
    { "loader/psd/0040_display_duplicate_reject", test_psd_color_0040 },
    { "loader/psd/0041_spot_lab_numeric", test_psd_color_0041 },
    { "loader/psd/0042_spot_gray_numeric", test_psd_color_0042 },
    { "loader/psd/0043_display_reverse_numeric", test_psd_color_0043 },
    { "loader/psd/0044_duotone_spot_reject", test_psd_color_0044 },
    { "loader/psd/0045_spot_lab_range_reject", test_psd_color_0045 },
    { "loader/psd/0046_alternate_id_numeric",
      test_psd_color_0046 },
    { "loader/psd/0047_alternate_order_numeric",
      test_psd_color_0047 },
    { "loader/psd/0048_alternate_partial_numeric",
      test_psd_color_0048 },
    { "loader/psd/0049_alternate_library_numeric",
      test_psd_color_0049 },
    { "loader/psd/0050_alternate_space_reject",
      test_psd_color_0050 },
    { "loader/psd/0051_alternate_missing_ids_reject",
      test_psd_color_0051 },
    { "loader/psd/0052_alternate_id_count_reject",
      test_psd_color_0052 },
    { "loader/psd/0053_alternate_id_duplicate_reject",
      test_psd_color_0053 },
    { "loader/psd/0054_alternate_duplicate_id_reject",
      test_psd_color_0054 },
    { "loader/psd/0055_alternate_unknown_id_reject",
      test_psd_color_0055 },
    { "loader/psd/0056_alternate_version_reject",
      test_psd_color_0056 },
    { "loader/psd/0057_alternate_count_reject",
      test_psd_color_0057 },
    { "loader/psd/0058_alternate_truncated_reject",
      test_psd_color_0058 },
    { "loader/psd/0059_alternate_resource_duplicate_reject",
      test_psd_color_0059 },
    { "loader/psd/0060_identifiers_truncated_reject",
      test_psd_color_0060 },
    { "loader/psd/0061_identifiers_duplicate_reject",
      test_psd_color_0061 },
    { "loader/psd/0062_alternate_empty_numeric",
      test_psd_color_0062 },
    { "loader/psd/0063_alternate_high_id_numeric",
      test_psd_color_0063 },
    { "loader/psd/0064_alternate_nondisplay_reject",
      test_psd_color_0064 },
    { "loader/psd/0065_alternate_nonspot_reject",
      test_psd_color_0065 },
    { "loader/psd/0066_hsb_red_numeric",
      test_psd_color_0066 },
    { "loader/psd/0067_hsb_yellow_numeric",
      test_psd_color_0067 },
    { "loader/psd/0068_hsb_green_numeric",
      test_psd_color_0068 },
    { "loader/psd/0069_hsb_cyan_numeric",
      test_psd_color_0069 },
    { "loader/psd/0070_hsb_blue_numeric",
      test_psd_color_0070 },
    { "loader/psd/0071_hsb_magenta_numeric",
      test_psd_color_0071 },
    { "loader/psd/0072_hsb_wrap_numeric",
      test_psd_color_0072 },
    { "loader/psd/0073_hsb_gray_numeric",
      test_psd_color_0073 },
    { "loader/psd/0074_hsb_mixed_numeric",
      test_psd_color_0074 },
    { "loader/psd/0075_cmyk_cyan_numeric",
      test_psd_color_0075 },
    { "loader/psd/0076_cmyk_red_numeric",
      test_psd_color_0076 },
    { "loader/psd/0077_cmyk_black_numeric",
      test_psd_color_0077 },
    { "loader/psd/0078_cmyk_gray_numeric",
      test_psd_color_0078 },
    { "loader/psd/0079_cmyk_mixed_numeric",
      test_psd_color_0079 },
    { "loader/psd/0080_alternate_count_limit_reject",
      test_psd_color_0080 },
    { "loader/psd/0081_identifiers_count_limit_reject",
      test_psd_color_0081 },
    { "loader/psd/0082_alternate_psb16_numeric",
      test_psd_color_0082 },
    { "loader/psd/0083_alternate_obsolete_numeric",
      test_psd_color_0083 },
    { "loader/psd/0084_alternate_lab_numeric",
      test_psd_color_0084 },
    { "loader/psd/0085_alternate_savedmask_reject",
      test_psd_color_0085 },
    { "loader/psd/0086_alternate_max_numeric", test_psd_color_0086 },
    { "loader/psd/0087_unused_identifiers_numeric", test_psd_color_0087 },
    { "loader/psd/0088_process_rgb_numeric",
      test_psd_color_0088 },
    { "loader/psd/0089_process_cmyk_numeric",
      test_psd_color_0089 },
    { "loader/psd/0090_process_rgb_savedmask_numeric",
      test_psd_color_0090 },
    { "loader/psd/0091_process_cmyk_savedmask_numeric",
      test_psd_color_0091 },
    { "loader/psd/0092_process_rgb16_numeric",
      test_psd_color_0092 },
    { "loader/psd/0093_process_cmyk16_numeric",
      test_psd_color_0093 },
    { "loader/psd/0094_process_rgb32_extended_numeric",
      test_psd_color_0094 },
    { "loader/psd/0095_process_cmyk32_numeric",
      test_psd_color_0095 },
    { "loader/psd/0096_process_rgb_icc_numeric",
      test_psd_color_0096 },
    { "loader/psd/0097_process_cmyk_icc_numeric",
      test_psd_color_0097 },
    { "loader/psd/0098_process_cmyk16_icc_numeric",
      test_psd_color_0098 },
    { "loader/psd/0099_process_cmyk32_icc_numeric",
      test_psd_color_0099 },
    { "loader/psd/0100_process_icc_disabled_numeric",
      test_psd_color_0100 },
    { "loader/psd/0101_process_icc_domain_reject",
      test_psd_color_0101 },
    { "loader/psd/0102_process_icc_malformed_reject",
      test_psd_color_0102 },
    { "loader/psd/0103_process_icc_unsupported_reject",
      test_psd_color_0103 },
    { "loader/psd/0104_process_rgb_byte_control",
      test_psd_color_0104 },
    { "loader/psd/0105_process_alpha_id_numeric",
      test_psd_color_0105 },
    { "loader/psd/0106_process_alpha_layer_numeric",
      test_psd_color_0106 },
    { "loader/psd/0107_process_alpha_mtrn_numeric",
      test_psd_color_0107 },
    { "loader/psd/0108_process_alpha_mt16_numeric",
      test_psd_color_0108 },
    { "loader/psd/0109_process_alpha_mt32_psb_numeric",
      test_psd_color_0109 },
    { "loader/psd/0110_process_alpha_lr16_numeric",
      test_psd_color_0110 },
    { "loader/psd/0111_process_display_count_reject",
      test_psd_color_0111 },
    { "loader/psd/0112_process_mask_alternate_reject",
      test_psd_color_0112 },
    { "loader/psd/0113_process_spot_nan_reject",
      test_psd_color_0113 },
    { "loader/psd/0114_process_rgb_nan_reject",
      test_psd_color_0114 },
    { "loader/psd/0115_process_cmyk_range_reject",
      test_psd_color_0115 },
    { "loader/psd/0116_process_plane_short_reject",
      test_psd_color_0116 },
    { "loader/psd/0117_process_rle_numeric",
      test_psd_color_0117 },
    { "loader/psd/0118_process_zip_numeric",
      test_psd_color_0118 },
    { "loader/psd/0119_process_prediction_numeric",
      test_psd_color_0119 },
    { "loader/psd/0120_process_psb16_rle_numeric",
      test_psd_color_0120 },
    { "loader/psd/0121_process_missing_composite_reject",
      test_psd_color_0121 },
    { "loader/psd/0122_process_tag_truncated_reject",
      test_psd_color_0122 },
    { "loader/psd/0123_process_savedmask_nan_numeric",
      test_psd_color_0123 },
    { "loader/psd/0124_process_identifier_duplicate_reject",
      test_psd_color_0124 },
    { "loader/psd/0125_process_real_spot_metadata_numeric",
      test_psd_color_0125 },
    { "loader/psd/0126_process_spot_order_numeric",
      test_psd_color_0126 },
    { "loader/psd/0127_process_alpha_rgb8_numeric",
      test_psd_color_0127 },
    { "loader/psd/0128_process_alpha_background_numeric",
      test_psd_color_0128 },
    { "loader/psd/0129_process_alpha_rgb16_numeric",
      test_psd_color_0129 },
    { "loader/psd/0130_process_alpha_rgb32_extended_numeric",
      test_psd_color_0130 },
    { "loader/psd/0131_process_alpha_p3_icc_numeric",
      test_psd_color_0131 },
    { "loader/psd/0132_process_alpha_p3_background_numeric",
      test_psd_color_0132 },
    { "loader/psd/0133_process_alpha_icc_disabled_numeric",
      test_psd_color_0133 },
    { "loader/psd/0134_process_alpha_omitted_display_numeric",
      test_psd_color_0134 },
    { "loader/psd/0135_process_alpha_omitted_ids_numeric",
      test_psd_color_0135 },
    { "loader/psd/0136_process_alpha_omitted_alternate_numeric",
      test_psd_color_0136 },
    { "loader/psd/0137_process_alpha_layer_no_ids_numeric",
      test_psd_color_0137 },
    { "loader/psd/0138_process_alpha_id_reordered_numeric",
      test_psd_color_0138 },
    { "loader/psd/0139_process_alpha_role_conflict_reject",
      test_psd_color_0139 },
    { "loader/psd/0140_process_alpha_spot_id_reject",
      test_psd_color_0140 },
    { "loader/psd/0141_process_alpha_omission_unmarked_reject",
      test_psd_color_0141 },
    { "loader/psd/0142_process_alpha_identifier_count_reject",
      test_psd_color_0142 },
    { "loader/psd/0143_process_alpha_duplicate_zero_reject",
      test_psd_color_0143 },
    { "loader/psd/0144_process_alpha_savedmask_nan_numeric",
      test_psd_color_0144 },
    { "loader/psd/0145_process_alpha_nan_reject",
      test_psd_color_0145 },
    { "loader/psd/0146_process_alpha_negative_reject",
      test_psd_color_0146 },
    { "loader/psd/0147_process_alpha_over_one_reject",
      test_psd_color_0147 },
    { "loader/psd/0148_process_alpha_tiny_white_numeric",
      test_psd_color_0148 },
    { "loader/psd/0149_process_alpha_unmatte_overflow_reject",
      test_psd_color_0149 },
    { "loader/psd/0150_process_alpha_unmatte_gamut_numeric",
      test_psd_color_0150 },
    { "loader/psd/0151_process_alpha_rle_numeric",
      test_psd_color_0151 },
    { "loader/psd/0152_process_alpha_zip16_numeric",
      test_psd_color_0152 },
    { "loader/psd/0153_process_alpha_prediction_numeric",
      test_psd_color_0153 },
    { "loader/psd/0154_process_alpha_psb32_numeric",
      test_psd_color_0154 },
    { "loader/psd/0155_process_alpha_spot_order_numeric",
      test_psd_color_0155 },
    { "loader/psd/0156_process_alpha_rgb16_icc_numeric",
      test_psd_color_0156 },
    { "loader/psd/0157_process_alpha_zero_background_numeric",
      test_psd_color_0157 },
    { "loader/psd/0158_process_alpha_cmyk_zero_numeric",
      test_psd_color_0158 },
    { "loader/psd/0159_gray8_spot_numeric", test_psd_color_0159 },
    { "loader/psd/0160_gray16_spot_numeric", test_psd_color_0160 },
    { "loader/psd/0161_gray32_spot_numeric", test_psd_color_0161 },
    { "loader/psd/0162_gray8_icc_numeric", test_psd_color_0162 },
    { "loader/psd/0163_gray16_icc_numeric", test_psd_color_0163 },
    { "loader/psd/0164_gray32_icc_numeric", test_psd_color_0164 },
    { "loader/psd/0165_gray_icc_disabled_numeric", test_psd_color_0165 },
    { "loader/psd/0166_gray_savedmask_numeric", test_psd_color_0166 },
    { "loader/psd/0167_gray_alternate_numeric", test_psd_color_0167 },
    { "loader/psd/0168_gray_icc_domain_reject", test_psd_color_0168 },
    { "loader/psd/0169_gray_icc_malformed_reject", test_psd_color_0169 },
    { "loader/psd/0170_gray_icc_unsupported_reject", test_psd_color_0170 },
    { "loader/psd/0171_gray_nan_reject", test_psd_color_0171 },
    { "loader/psd/0172_gray_alpha_zero_numeric", test_psd_color_0172 },
    { "loader/psd/0173_gray_rle_numeric", test_psd_color_0173 },
    { "loader/psd/0174_gray_zip_numeric", test_psd_color_0174 },
    { "loader/psd/0175_gray_psb_prediction_numeric", test_psd_color_0175 },
    { "loader/psd/0176_gray_byte_control", test_psd_color_0176 },
    { "loader/psd/0177_lab8_spot_numeric", test_psd_color_0177 },
    { "loader/psd/0178_lab16_spot_numeric", test_psd_color_0178 },
    { "loader/psd/0179_lab8_icc_numeric", test_psd_color_0179 },
    { "loader/psd/0180_lab16_icc_numeric", test_psd_color_0180 },
    { "loader/psd/0181_lab_icc_disabled_numeric", test_psd_color_0181 },
    { "loader/psd/0182_lab_savedmask_numeric", test_psd_color_0182 },
    { "loader/psd/0183_lab_alternate_numeric", test_psd_color_0183 },
    { "loader/psd/0184_lab_icc_domain_reject", test_psd_color_0184 },
    { "loader/psd/0185_lab_icc_malformed_reject", test_psd_color_0185 },
    { "loader/psd/0186_lab16_neutral_numeric", test_psd_color_0186 },
    { "loader/psd/0187_lab16_endpoints_numeric", test_psd_color_0187 },
    { "loader/psd/0188_lab32_spot_reject", test_psd_color_0188 },
    { "loader/psd/0189_lab_alpha_zero_numeric", test_psd_color_0189 },
    { "loader/psd/0190_lab_rle_numeric", test_psd_color_0190 },
    { "loader/psd/0191_lab_zip_numeric", test_psd_color_0191 },
    { "loader/psd/0192_lab_psb_prediction_numeric", test_psd_color_0192 },
    { "loader/psd/0193_lab_spot_order_numeric", test_psd_color_0193 },
    { "loader/psd/0194_gray_spot_order_numeric", test_psd_color_0194 },
    { "loader/psd/0195_lab_truncated_reject", test_psd_color_0195 },
    { "loader/psd/0196_gray_spot_nan_reject", test_psd_color_0196 },
    { "loader/psd/0197_lab_icc_effect_numeric", test_psd_color_0197 },
    { "loader/psd/0198_gray8_alpha_numeric", test_psd_color_0198 },
    { "loader/psd/0199_gray16_alpha_numeric", test_psd_color_0199 },
    { "loader/psd/0200_gray32_alpha_extended_numeric", test_psd_color_0200 },
    { "loader/psd/0201_gray8_alpha_icc_numeric", test_psd_color_0201 },
    { "loader/psd/0202_gray16_alpha_icc_numeric", test_psd_color_0202 },
    { "loader/psd/0203_gray32_alpha_icc_numeric", test_psd_color_0203 },
    { "loader/psd/0204_gray_alpha_icc_disabled_numeric", test_psd_color_0204 },
    { "loader/psd/0205_gray_alpha_background_numeric", test_psd_color_0205 },
    { "loader/psd/0206_gray_alpha_tiny_numeric", test_psd_color_0206 },
    { "loader/psd/0207_gray_alpha_overflow_reject", test_psd_color_0207 },
    { "loader/psd/0208_gray_alpha_nan_reject", test_psd_color_0208 },
    { "loader/psd/0209_gray_alpha_psb_prediction_numeric", test_psd_color_0209 },
    { "loader/psd/0210_gray_alpha_reordered_numeric", test_psd_color_0210 },
    { "loader/psd/0211_gray_alpha_display_omitted_numeric", test_psd_color_0211 },
    { "loader/psd/0212_gray_alpha_mask_nan_numeric", test_psd_color_0212 },
    { "loader/psd/0213_gray_alpha_spot_order_numeric", test_psd_color_0213 },
    { "loader/psd/0214_cmyk8_alpha_numeric", test_psd_color_0214 },
    { "loader/psd/0215_cmyk16_alpha_numeric", test_psd_color_0215 },
    { "loader/psd/0216_cmyk32_alpha_numeric", test_psd_color_0216 },
    { "loader/psd/0217_cmyk8_alpha_icc_numeric", test_psd_color_0217 },
    { "loader/psd/0218_cmyk16_alpha_icc_numeric", test_psd_color_0218 },
    { "loader/psd/0219_cmyk32_alpha_icc_numeric", test_psd_color_0219 },
    { "loader/psd/0220_cmyk_alpha_icc_disabled_numeric", test_psd_color_0220 },
    { "loader/psd/0221_cmyk_alpha_background_numeric", test_psd_color_0221 },
    { "loader/psd/0222_cmyk_alpha_stored_range_reject", test_psd_color_0222 },
    { "loader/psd/0223_cmyk_alpha_tiny_numeric", test_psd_color_0223 },
    { "loader/psd/0224_cmyk_alpha_nan_reject", test_psd_color_0224 },
    { "loader/psd/0225_cmyk_alpha_psb_zip_numeric", test_psd_color_0225 },
    { "loader/psd/0226_cmyk_alpha_psb_rle_numeric", test_psd_color_0226 },
    { "loader/psd/0227_cmyk_alpha_display_omitted_numeric", test_psd_color_0227 },
    { "loader/psd/0228_cmyk_alpha_reordered_numeric", test_psd_color_0228 },
    { "loader/psd/0229_cmyk_alpha_spot_order_numeric", test_psd_color_0229 },
    { "loader/psd/0230_gray_alpha_negative_reject", test_psd_color_0230 },
    { "loader/psd/0231_cmyk_alpha_negative_reject", test_psd_color_0231 },
    { "loader/psd/0232_gray_alpha_icc_domain_reject", test_psd_color_0232 },
    { "loader/psd/0233_cmyk_alpha_icc_domain_reject", test_psd_color_0233 },
    { "loader/psd/0234_gray_alpha_native16_numeric", test_psd_color_0234 },
    { "loader/psd/0235_cmyk_alpha_native8_numeric", test_psd_color_0235 },
    { "loader/psd/0236_lab8_alpha_numeric", test_psd_color_0236 },
    { "loader/psd/0237_lab16_alpha_numeric", test_psd_color_0237 },
    { "loader/psd/0238_lab8_alpha_icc_numeric", test_psd_color_0238 },
    { "loader/psd/0239_lab16_alpha_icc_numeric", test_psd_color_0239 },
    { "loader/psd/0240_lab_alpha_icc_effect_numeric", test_psd_color_0240 },
    { "loader/psd/0241_lab_alpha_icc_disabled_numeric", test_psd_color_0241 },
    { "loader/psd/0242_lab_alpha_background_numeric", test_psd_color_0242 },
    { "loader/psd/0243_lab_alpha_reordered_numeric", test_psd_color_0243 },
    { "loader/psd/0244_lab_alpha_display_omitted_numeric",
      test_psd_color_0244 },
    { "loader/psd/0245_lab_alpha_spot_order_numeric", test_psd_color_0245 },
    { "loader/psd/0246_lab_alpha_psb_rle_numeric", test_psd_color_0246 },
    { "loader/psd/0247_lab_alpha_psb_prediction_numeric", test_psd_color_0247 },
    { "loader/psd/0248_lab_alpha_zip_numeric", test_psd_color_0248 },
    { "loader/psd/0249_lab_alpha_neutral_numeric", test_psd_color_0249 },
    { "loader/psd/0250_lab_alpha_minimum_numeric", test_psd_color_0250 },
    { "loader/psd/0251_lab_alpha_icc_domain_reject", test_psd_color_0251 },
    { "loader/psd/0252_lab_alpha_native8_numeric", test_psd_color_0252 },
    { "loader/psd/0253_lab_alpha_native16_numeric", test_psd_color_0253 },
    { "loader/psd/0254_lab32_alpha_reject", test_psd_color_0254 },
    { "loader/psd/0255_lab_alpha_truncated_reject", test_psd_color_0255 },
    { "loader/psd/0256_lab_alpha_extended_numeric", test_psd_color_0256 },
    { "loader/psd/0257_lab_alpha_background_icc_numeric", test_psd_color_0257 },
    { "loader/psd/0258_lab_alpha_icc_malformed_reject", test_psd_color_0258 },
    { "loader/psd/0259_lab_alpha_psb8_prediction_numeric",
      test_psd_color_0259 },
    { "loader/psd/0260_lab_alpha_psd16_rle_numeric", test_psd_color_0260 },
    { "loader/psd/0261_duotone_table_native_precedence",
      test_psd_color_0261 },
    { "loader/psd/0262_duotone_table_unknown_native",
      test_psd_color_0262 },
    { "loader/psd/0263_gray_ink_white_numeric",
      test_psd_color_0263 },
    { "loader/psd/0264_gray_ink_black_numeric",
      test_psd_color_0264 },
    { "loader/psd/0265_gray_ink_quarter_numeric",
      test_psd_color_0265 },
    { "loader/psd/0266_gray_ink_dark_numeric",
      test_psd_color_0266 },
    { "loader/psd/0267_gray_ink_minimum_numeric",
      test_psd_color_0267 },
    { "loader/psd/0268_gray_ink_maximum_numeric",
      test_psd_color_0268 },
    { "loader/psd/0269_gray_ink_range_reject",
      test_psd_color_0269 },
    { "loader/psd/0270_gray_ink_unsigned_reject",
      test_psd_color_0270 },
    { "loader/psd/0271_gray_ink_legacy_numeric",
      test_psd_color_0271 },
    { "loader/psd/0272_gray_ink_alternate_numeric",
      test_psd_color_0272 },
    { "loader/psd/0273_gray_ink_rgb_process_numeric",
      test_psd_color_0273 },
    { "loader/psd/0274_gray_ink_gray_process_numeric",
      test_psd_color_0274 },
    { "loader/hdr/2258_hdr_xyze_signed_raw", test_hdr_2258 },
    { "loader/hdr/2259_hdr_xyze_signed_rle", test_hdr_2259 },
    { "loader/hdr/2260_hdr_xyze_signed_exposure", test_hdr_2260 },
    { "loader/hdr/2261_hdr_xyze_signed_colorcorr", test_hdr_2261 },
    { "loader/hdr/2262_hdr_xyze_reinhard_bounded", test_hdr_2262 },
    { "loader/hdr/2263_hdr_xyze_signed_saturation", test_hdr_2263 },
    { "loader/hdr/2264_hdr_xyze_float_limits", test_hdr_2264 },


    { "loader/0185_png_source_numeric",
      test_loader_0185_png_source_numeric },
    { "loader/libtiff/0142_rgb16_source", test_tiff_0142 },
    { "cms/0231_tiff_icc_numeric", test_cms_0231 },
    { "loader/0195_lcms_cmyk32_main", test_loader_0195_main },
    { "loader/0196_lcms_cmyk32_missing", test_loader_0196_missing },
    { "loader/0197_lcms_cmyk32_layers", test_loader_0197_layers },
    { "loader/0198_colorsync_cmyk8_channel_fallback",
      test_loader_0198_channels },
    { "loader/0199_colorsync_cmyk16_channel_fallback",
      test_loader_0199_channels },
    { "loader/0200_colorsync_cmyk32_profile_main",
      test_loader_0200_profile },
    { "loader/0201_colorsync_cmyk32_profile_missing",
      test_loader_0201_profile },
    { "loader/0202_colorsync_cmyk32_profile_layers",
      test_loader_0202_profile },
    { "loader/jpeg/0054_jp_dnl_shrink", test_loader_jp_0054 },
    { "loader/jpeg/0055_jp_dnl_grow_float", test_loader_jp_0055 },
    { "loader/jpeg/0056_jp_dnl_grow_12", test_loader_jp_0056 },
    { "loader/jpeg/0057_jp_dnl_grow_prog", test_loader_jp_0057 },
    { "loader/jpeg/0058_jp_dnl_grow_lossless", test_loader_jp_0058 },
    { "loader/jpeg/0059_jp_dnl_restart", test_loader_jp_0059 },
    { "loader/jpeg/0060_jp_dnl_stuffed", test_loader_jp_0060 },
    { "loader/jpeg/0061_jp_dnl_420", test_loader_jp_0061 },
    { "loader/jpeg/0062_jp_dnl_partial", test_loader_jp_0062 },
    { "loader/jpeg/0063_jp_dnl_missing_row", test_loader_jp_0063 },
    { "loader/jpeg/0064_jp_dnl_extra_row", test_loader_jp_0064 },
    { "loader/jpeg/0065_jp_dnl_duplicate", test_loader_jp_0065 },
    { "loader/jpeg/0066_jp_arith_gray_byte", test_loader_jp_0066 },
    { "loader/jpeg/0067_jp_arith_rgb_float", test_loader_jp_0067 },
    { "loader/jpeg/0068_jp_arith_seq_ac", test_loader_jp_0068 },
    { "loader/jpeg/0069_jp_arith_prog_ac", test_loader_jp_0069 },
    { "loader/jpeg/0070_jp_arith_custom_dac", test_loader_jp_0070 },
    { "loader/jpeg/0071_jp_arith_12_seq", test_loader_jp_0071 },
    { "loader/jpeg/0072_jp_arith_12_prog", test_loader_jp_0072 },
    { "loader/jpeg/0073_jp_arith_loss_p1_8_0", test_loader_jp_0073 },
    { "loader/jpeg/0074_jp_arith_loss_p2_8_0", test_loader_jp_0074 },
    { "loader/jpeg/0075_jp_arith_loss_p3_8_0", test_loader_jp_0075 },
    { "loader/jpeg/0076_jp_arith_loss_p4_8_0", test_loader_jp_0076 },
    { "loader/jpeg/0077_jp_arith_loss_p4_12_0", test_loader_jp_0077 },
    { "loader/jpeg/0078_jp_arith_loss_p4_16_0", test_loader_jp_0078 },
    { "loader/jpeg/0079_jp_arith_loss_p4_16_3", test_loader_jp_0079 },
    { "loader/jpeg/0080_jp_arith_loss_p4_2_0", test_loader_jp_0080 },
    { "loader/jpeg/0081_jp_arith_loss_p5_8_0", test_loader_jp_0081 },
    { "loader/jpeg/0082_jp_arith_loss_p6_8_0", test_loader_jp_0082 },
    { "loader/jpeg/0083_jp_arith_loss_p7_8_0", test_loader_jp_0083 },
    { "loader/jpeg/0084_jp_hier_huff_seq", test_loader_jp_0084 },
    { "loader/jpeg/0085_jp_hier_huff_prog", test_loader_jp_0085 },
    { "loader/jpeg/0086_jp_hier_loss_exp0", test_loader_jp_0086 },
    { "loader/jpeg/0087_jp_hier_loss_exp1", test_loader_jp_0087 },
    { "loader/jpeg/0088_jp_hier_loss_exp16", test_loader_jp_0088 },
    { "loader/jpeg/0089_jp_hier_loss_exp17", test_loader_jp_0089 },
    { "loader/jpeg/0090_jp_hier_arith_seq", test_loader_jp_0090 },
    { "loader/jpeg/0091_jp_hier_arith_prog", test_loader_jp_0091 },
    { "loader/jpeg/0092_jp_hier_arith_loss", test_loader_jp_0092 },
    { "loader/jpeg/0093_jp_truncated_eoi_reject", test_loader_jp_0093 },
    { "loader/jpeg/0094_jp_missing_eoi_reject", test_loader_jp_0094 },
    { "loader/jpeg/0095_jp_dac_odd_length_reject", test_loader_jp_0095 },
    { "loader/jpeg/0096_jp_dac_class_reject", test_loader_jp_0096 },
    { "loader/jpeg/0097_jp_dac_destination_reject", test_loader_jp_0097 },
    { "loader/jpeg/0098_jp_dac_bounds_reject", test_loader_jp_0098 },
    { "loader/jpeg/0099_jp_dac_zero_k_reject", test_loader_jp_0099 },
    { "loader/jpeg/0100_jp_dac_large_k_reject", test_loader_jp_0100 },
    { "loader/jpeg/0101_jp_missing_dqt_reject", test_loader_jp_0101 },
    { "loader/jpeg/0102_jp_duplicate_sof_reject", test_loader_jp_0102 },
    { "loader/jpeg/0103_jp_zero_sampling_reject", test_loader_jp_0103 },
    { "loader/jpeg/0104_jp_unknown_scan_component_reject", test_loader_jp_0104 },
    { "loader/jpeg/0105_jp_duplicate_scan_component_reject", test_loader_jp_0105 },
    { "loader/jpeg/0106_jp_interleaved_ac_scan_reject", test_loader_jp_0106 },
    { "loader/jpeg/0107_jp_dc_spectral_end_reject", test_loader_jp_0107 },
    { "loader/jpeg/0108_jp_refinement_gap_reject", test_loader_jp_0108 },
    { "loader/jpeg/0109_jp_refine_before_first_reject", test_loader_jp_0109 },
    { "loader/jpeg/0110_jp_excessive_approximation_reject", test_loader_jp_0110 },
    { "loader/jpeg/0111_jp_restart_length_reject", test_loader_jp_0111 },
    { "loader/jpeg/0112_jp_restart_sequence_reject", test_loader_jp_0112 },
    { "loader/jpeg/0113_jp_missing_restart_reject", test_loader_jp_0113 },
    { "loader/jpeg/0114_jp_entropy_eof_reject", test_loader_jp_0114 },
    { "loader/jpeg/0115_jp_hierarchy_missing_dhp_reject", test_loader_jp_0115 },
    { "loader/jpeg/0116_jp_duplicate_dhp_reject", test_loader_jp_0116 },
    { "loader/jpeg/0117_jp_hierarchy_precision_reject", test_loader_jp_0117 },
    { "loader/jpeg/0118_jp_incomplete_size_reject", test_loader_jp_0118 },
    { "loader/jpeg/0119_jp_expansion_factor_reject", test_loader_jp_0119 },
    { "loader/jpeg/0120_jp_duplicate_expansion_reject", test_loader_jp_0120 },
    { "loader/jpeg/0121_jp_expansion_before_scan_reject", test_loader_jp_0121 },
    { "loader/jpeg/0122_jp_differential_first_reject", test_loader_jp_0122 },
    { "loader/jpeg/0123_jp_unknown_frame_component_reject", test_loader_jp_0123 },
    { "loader/jpeg/0124_jp_differential_size_reject", test_loader_jp_0124 },
    { "loader/jpeg/0125_jp_trailing_expansion_reject", test_loader_jp_0125 },
    { "loader/jpeg/0126_jp_lossless_zero_predictor_reject", test_loader_jp_0126 },
    { "loader/jpeg/0127_jp_lossless_point_transform_reject", test_loader_jp_0127 },
    { "loader/jpeg/0128_jp_lossless_ac_selector_reject", test_loader_jp_0128 },
    { "loader/jpeg/0129_jp_lossless_predictor_range_reject", test_loader_jp_0129 },
    { "loader/jpeg/0130_jp_truncated_dac_reject", test_loader_jp_0130 },
    { "loader/jpeg/0131_jp_zero_quantizer_reject", test_loader_jp_0131 },
    { "loader/jpeg/0132_jp_truncated_dqt_reject", test_loader_jp_0132 },
    { "loader/jpeg/0133_jp_sequential_spectral_end_reject", test_loader_jp_0133 },
    { "loader/jpeg/0134_jp_unknown_trailing_marker_reject", test_loader_jp_0134 },
    { "loader/jpeg/0135_jp_arith_plain_cms", test_loader_jp_0135 },
    { "loader/jpeg/0136_jp_arith_adobe_cms", test_loader_jp_0136 },
    { "loader/jpeg/0137_jp_arith_ycck_cms", test_loader_jp_0137 },
    { "loader/jpeg/0138_jp_hier_plain_cms", test_loader_jp_0138 },
    { "loader/jpeg/0139_jp_hier_adobe_cms", test_loader_jp_0139 },
    { "loader/jpeg/0140_jp_hier_ycck_cms", test_loader_jp_0140 },
    { "loader/jpeg/0141_jp_arith_prog_ac_allocation", test_loader_jp_0141 },
    { "loader/jpeg/0142_jp_arith_loss_p4_16_0_allocation", test_loader_jp_0142 },
    { "loader/jpeg/0143_jp_hier_loss_exp17_allocation", test_loader_jp_0143 },
    { "loader/jpeg/0144_jp_hier_loss16_wrap", test_loader_jp_0144 },
    { "loader/jpeg/0145_jp_hier_rgb_groups", test_loader_jp_0145 },
    { "loader/jpeg/0146_jp_arith_rgb_loss8", test_loader_jp_0146 },
    { "loader/jpeg/0147_jp_arith_rgb_loss16", test_loader_jp_0147 },
    { "loader/jpeg/0148_jp_arith_default_dac", test_loader_jp_0148 },
    { "loader/jpeg/0149_jp_arith_prog_dnl_zero", test_loader_jp_0149 },
    { "loader/jpeg/0150_jp_arith_prog_dnl_redefine", test_loader_jp_0150 },
    { "loader/jpeg/0151_jp_arith_marker_fill", test_loader_jp_0151 },
    { "loader/jpeg/0152_jp_arith_loss_dnl_zero", test_loader_jp_0152 },
    { "loader/jpeg/0153_jp_arith_loss_dnl_redefine", test_loader_jp_0153 },
    { "loader/jpeg/0154_jp_hier_loss_dnl_zero", test_loader_jp_0154 },
    { "loader/jpeg/0155_jp_hier_arith_dnl_redefine", test_loader_jp_0155 },
    { "loader/jpeg/0163_jp_hier_arith_multiblock_seq", test_loader_jp_0163 },
    { "loader/jpeg/0164_jp_hier_arith_multiblock_prog", test_loader_jp_0164 },
    { "loader/jpeg/0165_jp_hier_short_entropy_reject", test_loader_jp_0165 },
    { "loader/jpeg/0166_sof3_category16_numeric",
      test_loader_jp_0166 },
    { "loader/jpeg/0167_sof3_modulo_numeric",
      test_loader_jp_0167 },
    { "loader/jpeg/0168_sof3_pred1_numeric",
      test_loader_jp_0168 },
    { "loader/jpeg/0169_sof3_pred2_numeric",
      test_loader_jp_0169 },
    { "loader/jpeg/0170_sof3_pred3_numeric",
      test_loader_jp_0170 },
    { "loader/jpeg/0171_sof3_pred4_numeric",
      test_loader_jp_0171 },
    { "loader/jpeg/0172_sof3_pred5_numeric",
      test_loader_jp_0172 },
    { "loader/jpeg/0173_sof3_pred6_numeric",
      test_loader_jp_0173 },
    { "loader/jpeg/0174_sof3_pred7_numeric",
      test_loader_jp_0174 },
    { "loader/jpeg/0175_sof3_pt3_pred5_numeric",
      test_loader_jp_0175 },
    { "loader/jpeg/0176_sof3_pt3_pred6_numeric",
      test_loader_jp_0176 },
    { "loader/jpeg/0177_sof3_pt3_pred7_numeric",
      test_loader_jp_0177 },
    { "loader/jpeg/0178_sof3_pt15_numeric",
      test_loader_jp_0178 },
    { "loader/jpeg/0179_sof3_precision2_numeric",
      test_loader_jp_0179 },
    { "loader/jpeg/0180_sof3_precision12_numeric",
      test_loader_jp_0180 },
    { "loader/jpeg/0181_sof3_restart16_numeric",
      test_loader_jp_0181 },
    { "loader/jpeg/0182_sof3_restart_pt3_numeric",
      test_loader_jp_0182 },
    { "loader/jpeg/0183_sof3_rgb_interleaved_numeric",
      test_loader_jp_0183 },
    { "loader/jpeg/0184_sof3_rgb_planar_numeric",
      test_loader_jp_0184 },
    { "loader/jpeg/0185_sof3_rgb_restart_pt3_numeric",
      test_loader_jp_0185 },
    { "loader/jpeg/0186_sof3_planar_restart_pt1_numeric",
      test_loader_jp_0186 },
    { "loader/jpeg/0187_sof3_column_numeric",
      test_loader_jp_0187 },
    { "loader/jpeg/0188_sof3_row_numeric",
      test_loader_jp_0188 },
    { "loader/jpeg/0189_sof3_dnl_zero_numeric",
      test_loader_jp_0189 },
    { "loader/jpeg/0190_sof3_dnl_redefine_numeric",
      test_loader_jp_0190 },
    { "loader/jpeg/0191_sof3_category16_last_numeric",
      test_loader_jp_0191 },
    { "loader/jpeg/0162_jp_arith_420_digest", test_loader_jp_0162 },
    { "loader/jpeg/0161_jp_arith_wide_prog", test_loader_jp_0161 },
    { "loader/jpeg/0160_jp_arith_wide_seq", test_loader_jp_0160 },
    { "loader/jpeg/0159_jp_hier_quant_redefinition", test_loader_jp_0159 },
    { "loader/jpeg/0158_jp_hier_signed_reference", test_loader_jp_0158 },
    { "loader/jpeg/0157_jp_hier_positive_clip", test_loader_jp_0157 },
    { "loader/jpeg/0156_jp_hier_negative_clip", test_loader_jp_0156 },
    { "loader/0203_jpeg_dnl_byte_numeric", test_loader_0203_jpeg_dnl },
    { "loader/0204_jpeg_dnl_float_numeric", test_loader_0204_jpeg_dnl },
    { "loader/0205_jpeg_dnl_12bit_numeric", test_loader_0205_jpeg_dnl },
    { "loader/0206_jpeg_dnl_progressive_numeric", test_loader_0206_jpeg_dnl },
    { "loader/0207_jpeg_dnl_lossless_numeric", test_loader_0207_jpeg_dnl },
    { "loader/0208_jpeg_dnl_restart_numeric", test_loader_0208_jpeg_dnl },
    { "loader/0209_jpeg_dnl_stuffed_numeric", test_loader_0209_jpeg_dnl },
    { "loader/0210_jpeg_dnl_420_digest", test_loader_0210_jpeg_dnl },
    { "loader/0211_jpeg_dnl_missing_reject", test_loader_0211_jpeg_dnl },
    { "loader/0212_jpeg_dnl_zero_reject", test_loader_0212_jpeg_dnl },
    { "loader/0213_jpeg_dnl_length_reject", test_loader_0213_jpeg_dnl },
    { "loader/0214_jpeg_dnl_truncated_reject", test_loader_0214_jpeg_dnl },
    { "loader/0215_jpeg_dnl_prescan_reject", test_loader_0215_jpeg_dnl },
    { "loader/0216_jpeg_dnl_late_reject", test_loader_0216_jpeg_dnl },
    { "loader/0217_jpeg_dnl_duplicate_reject", test_loader_0217_jpeg_dnl },
    { "loader/0218_jpeg_dnl_short_height_reject", test_loader_0218_jpeg_dnl },
    { "loader/0219_jpeg_dnl_tall_height_reject", test_loader_0219_jpeg_dnl },
    { "loader/0220_jpeg_dnl_restart_short_reject", test_loader_0220_jpeg_dnl },
    { "loader/0221_jpeg_dnl_extension_numeric", test_loader_0221_jpeg_dnl },
    { "loader/0222_jpeg_dnl_known_numeric", test_loader_0222_jpeg_dnl },
    { "loader/0223_jpeg_dnl_redefine_numeric", test_loader_0223_jpeg_dnl },
    { "loader/0224_jpeg_dnl_amplitude_reject", test_loader_0224_jpeg_dnl },
    { "loader/0225_jpeg_dnl_fill_numeric", test_loader_0225_jpeg_dnl },
    { "loader/0226_jpeg_dnl_eoi_reject", test_loader_0226_jpeg_dnl },
    { "loader/0227_jpeg_dnl_segment_reject", test_loader_0227_jpeg_dnl },
    { "loader/0228_tga_origin_rgb_raw_numeric",
      test_loader_0228_tga_origin },
    { "loader/0229_tga_origin_pal_raw_numeric",
      test_loader_0229_tga_origin },
    { "loader/0230_tga_origin_rgb_rle_numeric",
      test_loader_0230_tga_origin },
    { "loader/0231_tga_origin_pal_rle_numeric",
      test_loader_0231_tga_origin },
    { "loader/0232_tga_origin_index16_raw_numeric",
      test_loader_0232_tga_origin },
    { "loader/0233_tga_origin_index16_rle_numeric",
      test_loader_0233_tga_origin },
    { "loader/0234_tga_origin_byte_max_numeric",
      test_loader_0234_tga_origin },
    { "loader/0235_tga_origin_word_max_numeric",
      test_loader_0235_tga_origin },
    { "loader/0236_tga_origin_alpha_numeric",
      test_loader_0236_tga_origin },
    { "loader/0237_tga_origin_below_numeric",
      test_loader_0237_tga_origin },
    { "loader/0238_tga_origin_above_numeric",
      test_loader_0238_tga_origin },
    { "loader/0239_tga_origin_pal_fallback_numeric",
      test_loader_0239_tga_origin },
    { "loader/0240_tga_origin_gray_numeric",
      test_loader_0240_tga_origin },
    { "loader/0241_tga_origin_rgb16_numeric",
      test_loader_0241_tga_origin },
    { "loader/0172_lcms_lab_frame", test_loader_0172_lab },
    { "loader/0173_colorsync_lab_frame", test_loader_0173_lab },
    { "loader/0174_lab16_signed_values", test_loader_0174_lab16 },
    { "cms/0238_lcms_cmyk_linear", test_cms_0238 },
    { "cms/0239_lcms_cmyk_linear_alias", test_cms_0239 },
    { "cms/0240_lcms_cmyk_gamma", test_cms_0240 },
    { "cms/0241_lcms_cmyk_gamma_alias", test_cms_0241 },
    { "cms/0242_lcms_lab_linear", test_cms_0242 },
    { "cms/0243_lcms_lab_linear_alias", test_cms_0243 },
    { "cms/0244_lcms_lab_gamma", test_cms_0244 },
    { "cms/0245_lcms_lab_gamma_alias", test_cms_0245 },
    { "cms/0246_colorsync_lab_linear", test_cms_0246 },
    { "cms/0247_colorsync_lab_linear_alias", test_cms_0247 },
    { "cms/0248_colorsync_lab_gamma", test_cms_0248 },
    { "cms/0249_colorsync_lab_gamma_alias", test_cms_0249 },
    { "cms/0250_builtin_cmyk_float_linear",
      test_cms_0250 },
    { "cms/0251_builtin_cmyk_float_linear_alias",
      test_cms_0251 },
    { "cms/0252_builtin_cmyk_float_gamma",
      test_cms_0252 },
    { "cms/0253_builtin_cmyk_float_gamma_alias",
      test_cms_0253 },
    { "cms/0254_builtin_gray_byte_linear",
      test_cms_0254 },
    { "cms/0255_builtin_gray_byte_linear_alias",
      test_cms_0255 },
    { "cms/0256_builtin_gray_byte_gamma",
      test_cms_0256 },
    { "cms/0257_builtin_gray_byte_gamma_alias",
      test_cms_0257 },
    { "cms/0258_lcms_gray_byte_linear_alias_reject",
      test_cms_0258 },
    { "cms/0259_lcms_gray_byte_gamma_alias_reject",
      test_cms_0259 },
    { "cms/0260_lcms_rgb_byte_linear_alias_reject",
      test_cms_0260 },
    { "cms/0261_lcms_rgb_byte_gamma_alias_reject",
      test_cms_0261 },
    { "cms/0262_lcms_rgba_byte_linear_alias_reject",
      test_cms_0262 },
    { "cms/0263_lcms_rgba_byte_gamma_alias_reject",
      test_cms_0263 },
    { "cms/0264_lcms_cmyk_byte_linear_alias_reject",
      test_cms_0264 },
    { "cms/0265_lcms_cmyk_byte_gamma_alias_reject",
      test_cms_0265 },
    { "cms/0266_lcms_cmyk_word_linear_alias_reject",
      test_cms_0266 },
    { "cms/0267_lcms_cmyk_word_gamma_alias_reject",
      test_cms_0267 },
    { "cms/0268_lcms_gray_float_linear_alias_reject",
      test_cms_0268 },
    { "cms/0269_lcms_gray_float_gamma_alias_reject",
      test_cms_0269 },
    { "cms/0270_colorsync_gray8_linear",
      test_cms_0270 },
    { "cms/0271_colorsync_gray8_linear_alias",
      test_cms_0271 },
    { "cms/0272_colorsync_gray8_gamma",
      test_cms_0272 },
    { "cms/0273_colorsync_gray8_gamma_alias",
      test_cms_0273 },
    { "cms/0274_colorsync_gray8_channels_linear",
      test_cms_0274 },
    { "cms/0275_colorsync_gray8_channels_gamma",
      test_cms_0275 },
    { "cms/0276_colorsync_rgb8_channels_linear",
      test_cms_0276 },
    { "cms/0277_colorsync_rgb8_channels_gamma",
      test_cms_0277 },
    { "cms/0278_colorsync_rgba8_channels_linear",
      test_cms_0278 },
    { "cms/0279_colorsync_rgba8_channels_gamma",
      test_cms_0279 },
    { "cms/0280_colorsync_cmyk8_channels_linear",
      test_cms_0280 },
    { "cms/0281_colorsync_cmyk8_channels_gamma",
      test_cms_0281 },
    { "cms/0282_colorsync_cmyk16_channels_linear",
      test_cms_0282 },
    { "cms/0283_colorsync_cmyk16_channels_gamma",
      test_cms_0283 },
    { "cms/0284_colorsync_lab_float_channels_linear",
      test_cms_0284 },
    { "cms/0285_colorsync_lab_float_channels_gamma",
      test_cms_0285 },
    { "cms/0286_colorsync_channels_scope_linear",
      test_cms_0286 },
    { "cms/0287_colorsync_channels_scope_gamma",
      test_cms_0287 },
    { "cms/0288_builtin_gray8_profile_format_linear",
      test_cms_0288 },
    { "cms/0289_builtin_gray8_profile_format_gamma",
      test_cms_0289 },
    { "cms/0290_builtin_rgb8_profile_format_linear",
      test_cms_0290 },
    { "cms/0291_builtin_rgb8_profile_format_gamma",
      test_cms_0291 },
    { "cms/0292_builtin_rgba8_profile_format_linear",
      test_cms_0292 },
    { "cms/0293_builtin_rgba8_profile_format_gamma",
      test_cms_0293 },
    { "cms/0294_builtin_rgb_float_profile_format_linear",
      test_cms_0294 },
    { "cms/0295_builtin_rgb_float_profile_format_gamma",
      test_cms_0295 },
    { "cms/0296_builtin_cmyk8_profile_format_linear",
      test_cms_0296 },
    { "cms/0297_builtin_cmyk8_profile_format_gamma",
      test_cms_0297 },
    { "cms/0298_builtin_cmyk16_profile_format_linear",
      test_cms_0298 },
    { "cms/0299_builtin_cmyk16_profile_format_gamma",
      test_cms_0299 },
    { "cms/0300_builtin_cmyk_float_profile_format_linear",
      test_cms_0300 },
    { "cms/0301_builtin_cmyk_float_profile_format_gamma",
      test_cms_0301 },
    { "cms/0302_builtin_lab_float_profile_format_linear",
      test_cms_0302 },
    { "cms/0303_builtin_lab_float_profile_format_gamma",
      test_cms_0303 },
    { "cms/0304_builtin_gray_float_profile_format_linear",
      test_cms_0304 },
    { "cms/0305_builtin_gray_float_profile_format_gamma",
      test_cms_0305 },
    { "cms/0306_lcms2_gray8_profile_format_linear",
      test_cms_0306 },
    { "cms/0307_lcms2_gray8_profile_format_gamma",
      test_cms_0307 },
    { "cms/0308_lcms2_rgb8_profile_format_linear",
      test_cms_0308 },
    { "cms/0309_lcms2_rgb8_profile_format_gamma",
      test_cms_0309 },
    { "cms/0310_lcms2_rgba8_profile_format_linear",
      test_cms_0310 },
    { "cms/0311_lcms2_rgba8_profile_format_gamma",
      test_cms_0311 },
    { "cms/0312_lcms2_rgb_float_profile_format_linear",
      test_cms_0312 },
    { "cms/0313_lcms2_rgb_float_profile_format_gamma",
      test_cms_0313 },
    { "cms/0314_lcms2_cmyk8_profile_format_linear",
      test_cms_0314 },
    { "cms/0315_lcms2_cmyk8_profile_format_gamma",
      test_cms_0315 },
    { "cms/0316_lcms2_cmyk16_profile_format_linear",
      test_cms_0316 },
    { "cms/0317_lcms2_cmyk16_profile_format_gamma",
      test_cms_0317 },
    { "cms/0318_lcms2_cmyk_float_profile_format_linear",
      test_cms_0318 },
    { "cms/0319_lcms2_cmyk_float_profile_format_gamma",
      test_cms_0319 },
    { "cms/0320_lcms2_lab_float_profile_format_linear",
      test_cms_0320 },
    { "cms/0321_lcms2_lab_float_profile_format_gamma",
      test_cms_0321 },
    { "cms/0322_lcms2_gray_float_profile_format_linear",
      test_cms_0322 },
    { "cms/0323_lcms2_gray_float_profile_format_gamma",
      test_cms_0323 },
    { "cms/0324_colorsync_gray8_profile_format_linear",
      test_cms_0324 },
    { "cms/0325_colorsync_gray8_profile_format_gamma",
      test_cms_0325 },
    { "cms/0326_colorsync_rgb8_profile_format_linear",
      test_cms_0326 },
    { "cms/0327_colorsync_rgb8_profile_format_gamma",
      test_cms_0327 },
    { "cms/0328_colorsync_rgba8_profile_format_linear",
      test_cms_0328 },
    { "cms/0329_colorsync_rgba8_profile_format_gamma",
      test_cms_0329 },
    { "cms/0330_colorsync_rgb_float_profile_format_linear",
      test_cms_0330 },
    { "cms/0331_colorsync_rgb_float_profile_format_gamma",
      test_cms_0331 },
    { "cms/0332_colorsync_cmyk8_profile_format_linear",
      test_cms_0332 },
    { "cms/0333_colorsync_cmyk8_profile_format_gamma",
      test_cms_0333 },
    { "cms/0334_colorsync_cmyk16_profile_format_linear",
      test_cms_0334 },
    { "cms/0335_colorsync_cmyk16_profile_format_gamma",
      test_cms_0335 },
    { "cms/0336_colorsync_cmyk_float_profile_format_linear",
      test_cms_0336 },
    { "cms/0337_colorsync_cmyk_float_profile_format_gamma",
      test_cms_0337 },
    { "cms/0338_colorsync_lab_float_profile_format_linear",
      test_cms_0338 },
    { "cms/0339_colorsync_lab_float_profile_format_gamma",
      test_cms_0339 },
    { "cms/0340_colorsync_gray_float_profile_format_linear",
      test_cms_0340 },
    { "cms/0341_colorsync_gray_float_profile_format_gamma",
      test_cms_0341 },
    { "cms/0342_builtin_profile_format_scope",
      test_cms_0342 },
    { "cms/0343_colorsync_profile_format_scope",
      test_cms_0343 },
    { "cms/0344_lab_linear_profile_builtin",
      test_cms_0344_lab_linear },
    { "cms/0345_lab_linear_profile_lcms2",
      test_cms_0345_lab_linear },
    { "cms/0346_lab_linear_profile_colorsync",
      test_cms_0346_lab_linear },
    { "cms/0347_lab_linear_late_builtin",
      test_cms_0347_lab_linear },
    { "cms/0348_lab_linear_late_colorsync",
      test_cms_0348_lab_linear },
    { "cms/0349_lab_linear_allocation_failure",
      test_cms_0349_lab_linear },
    { "cms/0350_lab_linear_nan_result",
      test_cms_0350_lab_linear },
    { "cms/0351_lab_linear_infinite_result",
      test_cms_0351_lab_linear },
    { "cms/0352_mixed_gray_curv",
      test_cms_0352_mixed },
    { "cms/0353_mixed_gray_para0",
      test_cms_0353_mixed },
    { "cms/0354_mixed_rgb_curv",
      test_cms_0354_mixed },
    { "cms/0355_mixed_rgb_para0_strict",
      test_cms_0355_mixed },
    { "cms/0356_mixed_rgb_reuse_generic",
      test_cms_0356_mixed },
    { "cms/0357_mixed_rgb_native_strict",
      test_cms_0357_mixed },
    { "cms/0358_mixed_gray_reuse",
      test_cms_0358_mixed },
    { "cms/0359_mixed_display_p3",
      test_cms_0359_mixed },
    { "cms/0360_mixed_pack_allocation",
      test_cms_0360_mixed },
    { "cms/0361_mixed_output_allocation",
      test_cms_0361_mixed },
    { "cms/0362_mixed_first_create",
      test_cms_0362_mixed },
    { "cms/0363_mixed_second_create",
      test_cms_0363_mixed },
    { "cms/0364_mixed_late_apply",
      test_cms_0364_mixed },
    { "cms/0365_mixed_second_nonfinite",
      test_cms_0365_mixed },
    { "cms/0366_mixed_first_nonfinite",
      test_cms_0366_mixed },
    { "cms/0367_mixed_second_permutation_allocation",
      test_cms_0367_mixed },
    { "loader/0184_lab_float_linear_builtin",
      test_loader_0184_lab_linear },
    { "loader/0185_lab_float_linear_lcms2",
      test_loader_0185_lab_linear },
    { "loader/0186_lab_float_linear_colorsync",
      test_loader_0186_lab_linear },
    { "loader/0187_lab_word_linear_builtin",
      test_loader_0187_lab_linear },
    { "loader/0188_lab_word_linear_lcms2",
      test_loader_0188_lab_linear },
    { "loader/0189_lab_word_linear_colorsync",
      test_loader_0189_lab_linear },
    { "loader/0190_lab_float_late_builtin",
      test_loader_0190_lab_linear },
    { "loader/0191_lab_float_late_colorsync",
      test_loader_0191_lab_linear },
    { "loader/0192_lab_float_allocation_failure",
      test_loader_0192_lab_linear },
    { "loader/0193_lab_float_nan_result",
      test_loader_0193_lab_linear },
    { "loader/0194_lab_float_infinite_result",
      test_loader_0194_lab_linear },
    { "loader/0195_lab_word_late_builtin",
      test_loader_0195_lab_linear },
    { "loader/0196_lab_word_late_colorsync",
      test_loader_0196_lab_linear },
    { "loader/0197_lab_word_allocation_failure",
      test_loader_0197_lab_linear },
    { "loader/0198_lab_word_nan_result",
      test_loader_0198_lab_linear },
    { "loader/0199_lab_word_infinite_result",
      test_loader_0199_lab_linear },
    { "loader/0200_logluv32_linear",
      test_loader_0200_logluv },
    { "loader/0201_logluv24_linear",
      test_loader_0201_logluv },
    { "loader/0202_logluv_rows2",
      test_loader_0202_logluv },
    { "loader/0203_logluv_orientation2",
      test_loader_0203_logluv },
    { "loader/0204_logluv_orientation3",
      test_loader_0204_logluv },
    { "loader/0205_logluv_orientation4",
      test_loader_0205_logluv },
    { "loader/0206_logluv_cms_none",
      test_loader_0206_logluv },
    { "loader/0207_logluv_icc_ignored",
      test_loader_0207_logluv },
    { "loader/0208_logluv_whitepoint_ignored",
      test_loader_0208_logluv },
    { "loader/0209_logluv_stonits_scale",
      test_loader_0209_logluv },
    { "loader/0210_logluv_gamma_target",
      test_loader_0210_logluv },
    { "loader/0211_logluv_byte_target",
      test_loader_0211_logluv },
    { "loader/0212_logluv_cielab_target",
      test_loader_0212_logluv },
    { "loader/0213_logluv_oklab_target",
      test_loader_0213_logluv },
    { "loader/0214_logluv_din99d_target",
      test_loader_0214_logluv },
    { "loader/0215_logluv_tile_fallback",
      test_loader_0215_logluv },
    { "loader/0216_logluv_lcms_no_apply",
      test_loader_0216_logluv },
    { "loader/0217_logluv_colorsync_no_apply",
      test_loader_0217_logluv },
    { "loader/0218_logluv_float_request_failure",
      test_loader_0218_logluv },
    { "loader/0219_logluv_interface_failure",
      test_loader_0219_logluv },
    { "loader/0220_logluv_scanline_size_failure",
      test_loader_0220_logluv },
    { "loader/0221_logluv_row_allocation_failure",
      test_loader_0221_logluv },
    { "loader/0222_logluv_image_allocation_failure",
      test_loader_0222_logluv },
    { "loader/0223_logluv_late_read_failure",
      test_loader_0223_logluv },
    { "loader/0224_logluv_nan_rejected",
      test_loader_0224_logluv },
    { "loader/0225_logluv_infinity_rejected",
      test_loader_0225_logluv },
    { "loader/0226_logluv_target_failure",
      test_loader_0226_logluv },
    { "loader/0227_float_rgb_le",
      test_loader_0227_tfloat },
    { "loader/0228_float_rgb_be",
      test_loader_0228_tfloat },
    { "loader/0229_float_black_le",
      test_loader_0229_tfloat },
    { "loader/0230_float_black_be",
      test_loader_0230_tfloat },
    { "loader/0231_float_white_le",
      test_loader_0231_tfloat },
    { "loader/0232_float_white_be",
      test_loader_0232_tfloat },
    { "loader/0233_float_rgb_orientation3",
      test_loader_0233_tfloat },
    { "loader/0234_float_black_orientation2",
      test_loader_0234_tfloat },
    { "loader/0235_float_white_orientation4",
      test_loader_0235_tfloat },
    { "loader/0236_float_range_tags",
      test_loader_0236_tfloat },
    { "loader/0237_float_none_with_icc",
      test_loader_0237_tfloat },
    { "loader/0238_float_rgb_invalid_source",
      test_loader_0238_tfloat },
    { "loader/0239_float_black_invalid_source",
      test_loader_0239_tfloat },
    { "loader/0240_float_white_mismatch_source",
      test_loader_0240_tfloat },
    { "loader/0241_float_dimensions_control",
      test_loader_0241_tfloat },
    { "loader/0242_float_gamma_target",
      test_loader_0242_tfloat },
    { "loader/0243_float_byte_preference",
      test_loader_0243_tfloat },
    { "loader/0244_float_oklab_target",
      test_loader_0244_tfloat },
    { "loader/0245_float_rgb_late_nan",
      test_loader_0245_tfloat },
    { "loader/0246_float_black_late_infinity",
      test_loader_0246_tfloat },
    { "loader/0247_float_white_late_infinity",
      test_loader_0247_tfloat },
    { "loader/0248_float_early_failure_control",
      test_loader_0248_tfloat },
    { "loader/0249_float_late_read_failure",
      test_loader_0249_tfloat },
    { "loader/0250_managed_rgb_builtin",
      test_loader_0250_mfloat },
    { "loader/0251_managed_rgb_lcms2",
      test_loader_0251_mfloat },
    { "loader/0252_managed_rgb_colorsync",
      test_loader_0252_mfloat },
    { "loader/0253_managed_black_builtin",
      test_loader_0253_mfloat },
    { "loader/0254_managed_black_lcms2",
      test_loader_0254_mfloat },
    { "loader/0255_managed_black_colorsync",
      test_loader_0255_mfloat },
    { "loader/0256_managed_white_builtin",
      test_loader_0256_mfloat },
    { "loader/0257_managed_white_lcms2",
      test_loader_0257_mfloat },
    { "loader/0258_managed_white_colorsync",
      test_loader_0258_mfloat },
    { "loader/0259_managed_rgb_be_o3",
      test_loader_0259_mfloat },
    { "loader/0260_managed_white_be_o4",
      test_loader_0260_mfloat },
    { "loader/0261_managed_table_builtin",
      test_loader_0261_mfloat },
    { "loader/0262_managed_table_lcms2",
      test_loader_0262_mfloat },
    { "loader/0263_managed_rgb_late_builtin",
      test_loader_0263_mfloat },
    { "loader/0264_managed_black_late_builtin",
      test_loader_0264_mfloat },
    { "loader/0265_managed_white_late_colorsync",
      test_loader_0265_mfloat },
    { "loader/0266_managed_rgb_late_colorsync",
      test_loader_0266_mfloat },
    { "loader/0267_managed_rgb_nan_lcms2",
      test_loader_0267_mfloat },
    { "loader/0268_managed_black_nan_colorsync",
      test_loader_0268_mfloat },
    { "loader/0269_managed_early_control",
      test_loader_0269_mfloat },
    { "loader/0270_managed_rgb_input_nan",
      test_loader_0270_mfloat },
    { "loader/0271_managed_black_input_infinity",
      test_loader_0271_mfloat },
    { "loader/0272_managed_white_input_infinity",
      test_loader_0272_mfloat },
    { "loader/0273_managed_gray_native_gamma",
      test_loader_0273_mfloat },
    { "loader/0274_managed_gray_mixed_default",
      test_loader_0274_mfloat },
    { "loader/0275_managed_gray_mixed_native",
      test_loader_0275_mfloat },
    { "loader/0276_managed_mixed_builtin",
      test_loader_0276_mfloat },
    { "loader/0277_managed_mixed_lcms2",
      test_loader_0277_mfloat },
    { "loader/0278_managed_extended_lcms2",
      test_loader_0278_mfloat },
    { "loader/0279_managed_dimensions_control",
      test_loader_0279_mfloat },
    { "loader/0280_gray16_white_builtin",
      test_loader_0280_gray16 },
    { "loader/0281_gray16_black_lcms2",
      test_loader_0281_gray16 },
    { "loader/0282_gray16_black_colorsync",
      test_loader_0282_gray16 },
    { "loader/0283_gray16_white_late_builtin",
      test_loader_0283_gray16 },
    { "loader/0284_gray16_black_nan_lcms2",
      test_loader_0284_gray16 },
    { "loader/0285_byte_rgb_builtin",
      test_loader_0285_byte },
    { "loader/0286_byte_rgb_lcms2",
      test_loader_0286_byte },
    { "loader/0287_byte_rgb_colorsync",
      test_loader_0287_byte },
    { "loader/0288_byte_black_builtin",
      test_loader_0288_byte },
    { "loader/0289_byte_black_lcms2",
      test_loader_0289_byte },
    { "loader/0290_byte_black_colorsync",
      test_loader_0290_byte },
    { "loader/0291_byte_white_builtin",
      test_loader_0291_byte },
    { "loader/0292_byte_white_lcms2",
      test_loader_0292_byte },
    { "loader/0293_byte_white_colorsync",
      test_loader_0293_byte },
    { "loader/0294_byte_rgb_fallback",
      test_loader_0294_byte },
    { "loader/0295_byte_black_fallback",
      test_loader_0295_byte },
    { "loader/0296_byte_white_fallback",
      test_loader_0296_byte },
    { "loader/0297_byte_orientation_void",
      test_loader_0297_byte },
    { "loader/0298_byte_gamma_target",
      test_loader_0298_byte },
    { "loader/0299_byte_final_byte_target",
      test_loader_0299_byte },
    { "loader/0300_byte_rgb_late_failure",
      test_loader_0300_byte },
    { "loader/0301_byte_black_late_failure",
      test_loader_0301_byte },
    { "loader/0302_byte_white_late_failure",
      test_loader_0302_byte },
    { "loader/0303_byte_decode_allocation_failure",
      test_loader_0303_byte },
    { "loader/0304_byte_decode_read_failure",
      test_loader_0304_byte },
    { "loader/0305_alpha_rgb_builtin",
      test_loader_0305_alpha },
    { "loader/0306_alpha_rgb_lcms2",
      test_loader_0306_alpha },
    { "loader/0307_alpha_rgb_colorsync",
      test_loader_0307_alpha },
    { "loader/0308_alpha_black_builtin",
      test_loader_0308_alpha },
    { "loader/0309_alpha_black_lcms2",
      test_loader_0309_alpha },
    { "loader/0310_alpha_black_colorsync",
      test_loader_0310_alpha },
    { "loader/0311_alpha_white_builtin",
      test_loader_0311_alpha },
    { "loader/0312_alpha_white_lcms2",
      test_loader_0312_alpha },
    { "loader/0313_alpha_white_colorsync",
      test_loader_0313_alpha },
    { "loader/0314_alpha_rgb_fallback",
      test_loader_0314_alpha },
    { "loader/0315_alpha_black_fallback",
      test_loader_0315_alpha },
    { "loader/0316_alpha_white_fallback",
      test_loader_0316_alpha },
    { "loader/0317_alpha_orientation_void",
      test_loader_0317_alpha },
    { "loader/0318_alpha_policy",
      test_loader_0318_alpha },
    { "loader/0319_alpha_gamma_target",
      test_loader_0319_alpha },
    { "loader/0320_alpha_final_byte_target",
      test_loader_0320_alpha },
    { "loader/0321_alpha_working_targets",
      test_loader_0321_alpha },
    { "loader/0322_alpha_rgb_builtin_failure",
      test_loader_0322_alpha },
    { "loader/0323_alpha_rgb_lcms2_failure",
      test_loader_0323_alpha },
    { "loader/0324_alpha_rgb_colorsync_failure",
      test_loader_0324_alpha },
    { "loader/0325_alpha_black_builtin_failure",
      test_loader_0325_alpha },
    { "loader/0326_alpha_black_lcms2_failure",
      test_loader_0326_alpha },
    { "loader/0327_alpha_black_colorsync_failure",
      test_loader_0327_alpha },
    { "loader/0328_alpha_white_builtin_failure",
      test_loader_0328_alpha },
    { "loader/0329_alpha_white_lcms2_failure",
      test_loader_0329_alpha },
    { "loader/0330_alpha_white_colorsync_failure",
      test_loader_0330_alpha },
    { "loader/0331_alpha_row_allocation_failure",
      test_loader_0331_alpha },
    { "loader/0332_alpha_color_allocation_failure",
      test_loader_0332_alpha },
    { "loader/0333_alpha_plane_allocation_failure",
      test_loader_0333_alpha },
    { "loader/0334_alpha_late_scanline_failure",
      test_loader_0334_alpha },
    { "loader/0335_alpha16_rgb_builtin",
      test_loader_0335_alpha16 },
    { "loader/0336_alpha16_rgb_lcms2",
      test_loader_0336_alpha16 },
    { "loader/0337_alpha16_rgb_colorsync",
      test_loader_0337_alpha16 },
    { "loader/0338_alpha16_black_builtin",
      test_loader_0338_alpha16 },
    { "loader/0339_alpha16_black_lcms2",
      test_loader_0339_alpha16 },
    { "loader/0340_alpha16_black_colorsync",
      test_loader_0340_alpha16 },
    { "loader/0341_alpha16_white_builtin",
      test_loader_0341_alpha16 },
    { "loader/0342_alpha16_white_lcms2",
      test_loader_0342_alpha16 },
    { "loader/0343_alpha16_white_colorsync",
      test_loader_0343_alpha16 },
    { "loader/0344_alpha16_rgb_fallback",
      test_loader_0344_alpha16 },
    { "loader/0345_alpha16_black_fallback",
      test_loader_0345_alpha16 },
    { "loader/0346_alpha16_white_fallback",
      test_loader_0346_alpha16 },
    { "loader/0347_alpha16_orientation_void",
      test_loader_0347_alpha16 },
    { "loader/0348_alpha16_policy",
      test_loader_0348_alpha16 },
    { "loader/0349_alpha16_gamma_target",
      test_loader_0349_alpha16 },
    { "loader/0350_alpha16_final_byte_target",
      test_loader_0350_alpha16 },
    { "loader/0351_alpha16_working_targets",
      test_loader_0351_alpha16 },
    { "loader/0352_alpha16_rgb_builtin_failure",
      test_loader_0352_alpha16 },
    { "loader/0353_alpha16_rgb_lcms2_failure",
      test_loader_0353_alpha16 },
    { "loader/0354_alpha16_rgb_colorsync_failure",
      test_loader_0354_alpha16 },
    { "loader/0355_alpha16_black_builtin_failure",
      test_loader_0355_alpha16 },
    { "loader/0356_alpha16_black_lcms2_failure",
      test_loader_0356_alpha16 },
    { "loader/0357_alpha16_black_colorsync_failure",
      test_loader_0357_alpha16 },
    { "loader/0358_alpha16_white_builtin_failure",
      test_loader_0358_alpha16 },
    { "loader/0359_alpha16_white_lcms2_failure",
      test_loader_0359_alpha16 },
    { "loader/0360_alpha16_white_colorsync_failure",
      test_loader_0360_alpha16 },
    { "loader/0361_alpha16_row_allocation_failure",
      test_loader_0361_alpha16 },
    { "loader/0362_alpha16_color_allocation_failure",
      test_loader_0362_alpha16 },
    { "loader/0363_alpha16_plane_allocation_failure",
      test_loader_0363_alpha16 },
    { "loader/0364_alpha16_late_scanline_failure",
      test_loader_0364_alpha16 },
    { "loader/0365_alpha_float32_rejected",
      test_loader_0365_alpha16 },
    { "loader/0366_legacy_builtin_success",
      test_loader_0366_legacy },
    { "loader/0367_legacy_lcms2_success",
      test_loader_0367_legacy },
    { "loader/0368_legacy_colorsync_success",
      test_loader_0368_legacy },
    { "loader/0369_legacy_rgb_builtin_failure",
      test_loader_0369_legacy },
    { "loader/0370_legacy_rgb_lcms2_failure",
      test_loader_0370_legacy },
    { "loader/0371_legacy_rgb_colorsync_failure",
      test_loader_0371_legacy },
    { "loader/0372_legacy_gray_builtin_failure",
      test_loader_0372_legacy },
    { "loader/0373_legacy_gray_lcms2_failure",
      test_loader_0373_legacy },
    { "loader/0374_legacy_gray_colorsync_failure",
      test_loader_0374_legacy },
    { "loader/0375_legacy_rgb_profile_guard",
      test_loader_0375_legacy },
    { "loader/0376_legacy_gray_profile_guard",
      test_loader_0376_legacy },
    { "loader/0377_legacy_rgb_allocation_failure",
      test_loader_0377_legacy },
    { "loader/0378_legacy_gray_input_allocation_failure",
      test_loader_0378_legacy },
    { "loader/0379_legacy_gray_output_allocation_failure",
      test_loader_0379_legacy },
    { "loader/0380_legacy_rgb_transform_failure",
      test_loader_0380_legacy },
    { "loader/0381_legacy_gray_transform_failure",
      test_loader_0381_legacy },
    { "loader/0382_legacy_alpha_preserved",
      test_loader_0382_legacy },
    { "loader/0413_layout_rgb8_tile",
      test_loader_0413_layout },
    { "loader/0414_layout_rgb16_tile",
      test_loader_0414_layout },
    { "loader/0415_layout_black8_tile",
      test_loader_0415_layout },
    { "loader/0416_layout_black16_tile",
      test_loader_0416_layout },
    { "loader/0417_layout_white8_tile",
      test_loader_0417_layout },
    { "loader/0418_layout_white16_tile",
      test_loader_0418_layout },
    { "loader/0419_layout_rgb8_planar",
      test_loader_0419_layout },
    { "loader/0420_layout_rgb8_o5",
      test_loader_0420_layout },
    { "loader/0421_layout_rgb8_o6",
      test_loader_0421_layout },
    { "loader/0422_layout_rgb8_o7",
      test_loader_0422_layout },
    { "loader/0423_layout_rgb8_o8",
      test_loader_0423_layout },
    { "loader/0424_layout_rgb16_o6",
      test_loader_0424_layout },
    { "loader/0425_layout_white16_o8",
      test_loader_0425_layout },
    { "loader/0426_layout_black1_levels",
      test_loader_0426_layout },
    { "loader/0427_layout_black2_levels",
      test_loader_0427_layout },
    { "loader/0428_layout_black4_levels",
      test_loader_0428_layout },
    { "loader/0429_layout_white1_levels",
      test_loader_0429_layout },
    { "loader/0430_layout_white2_levels",
      test_loader_0430_layout },
    { "loader/0431_layout_white4_levels",
      test_loader_0431_layout },
    { "loader/0432_layout_palette4_map16",
      test_loader_0432_layout },
    { "loader/0433_layout_rgb8_tile_packbits",
      test_loader_0433_layout },
    { "loader/0434_layout_rgb16_tile_packbits",
      test_loader_0434_layout },
    { "loader/0435_layout_black8_tile_packbits",
      test_loader_0435_layout },
    { "loader/0436_layout_black16_tile_packbits",
      test_loader_0436_layout },
    { "loader/0437_layout_white8_tile_packbits",
      test_loader_0437_layout },
    { "loader/0438_layout_white16_tile_packbits",
      test_loader_0438_layout },
    { "loader/0439_layout_rgb8_planar_alpha",
      test_loader_0439_layout },
    { "loader/0440_layout_rgb16_tile_mismatch",
      test_loader_0440_layout },
    { "loader/0441_layout_black4_levels_mismatch",
      test_loader_0441_layout },
    { "loader/0442_layout_palette4_map16_mismatch",
      test_loader_0442_layout },
    { "loader/0443_layout_rgb16_tile_absent",
      test_loader_0443_layout },
    { "loader/0444_layout_rgb16_tile_invalid",
      test_loader_0444_layout },
    { "loader/0445_layout_rgb16_multitile_alpha",
      test_loader_0445_layout },
    { "loader/0446_layout_palette16_index",
      test_loader_0446_layout },
    { "loader/0447_layout_white2_fillorder2",
      test_loader_0447_layout },
    { "loader/0448_layout_lcms2",
      test_loader_0448_layout },
    { "loader/0449_layout_colorsync",
      test_loader_0449_layout },
    { "loader/0450_layout_geometry_target",
      test_loader_0450_layout },
    { "loader/0451_layout_gray_palette_target",
      test_loader_0451_layout },
    { "loader/0452_layout_alpha_target",
      test_loader_0452_layout },
    { "loader/0453_layout_buffer_allocation",
      test_loader_0453_layout },
    { "loader/0454_layout_color_allocation",
      test_loader_0454_layout },
    { "loader/0455_layout_alpha_allocation",
      test_loader_0455_layout },
    { "loader/0456_layout_zero_row_size",
      test_loader_0456_layout },
    { "loader/0457_layout_zero_tile_size",
      test_loader_0457_layout },
    { "loader/0458_layout_short_tile_row",
      test_loader_0458_layout },
    { "loader/0459_layout_later_row",
      test_loader_0459_layout },
    { "loader/0460_layout_later_plane",
      test_loader_0460_layout },
    { "loader/0461_layout_later_tile",
      test_loader_0461_layout },
    { "loader/0462_layout_short_tile_decode",
      test_loader_0462_layout },
    { "loader/0463_layout_rgb_cms_fallback",
      test_loader_0463_layout },
    { "loader/0464_layout_gray_cms_fallback",
      test_loader_0464_layout },
    { "loader/0465_layout_palette_cms_fallback",
      test_loader_0465_layout },
    { "loader/0466_layout_alpha_cms_fallback",
      test_loader_0466_layout },
    { "loader/0467_ycbcr_builtin",
      test_loader_0467_ycbcr },
    { "loader/0468_ycbcr_lcms2",
      test_loader_0468_ycbcr },
    { "loader/0469_ycbcr_colorsync",
      test_loader_0469_ycbcr },
    { "loader/0470_ycbcr_profile_guard",
      test_loader_0470_ycbcr },
    { "loader/0471_ycbcr_prefix_failure",
      test_loader_0471_ycbcr },
    { "loader/0472_ycbcr_allocation_failure",
      test_loader_0472_ycbcr },
    { "loader/0473_ycbcr_profile_failure",
      test_loader_0473_ycbcr },
    { "loader/0474_ycbcr_targets",
      test_loader_0474_ycbcr },
    { "loader/0558_device_lab8_associated", test_loader_0558_device },
    { "loader/0494_device_cielab8_strip_builtin", test_loader_0494_device },
    { "loader/0495_device_cielab8_separate_builtin", test_loader_0495_device },
    { "loader/0496_device_cielab8_tile_builtin", test_loader_0496_device },
    { "loader/0497_device_cielab16_tile_builtin", test_loader_0497_device },
    { "loader/0498_device_icclab8_separate_builtin", test_loader_0498_device },
    { "loader/0499_device_itulab8_tile_builtin", test_loader_0499_device },
    { "loader/0500_device_cmyk8_strip_builtin", test_loader_0500_device },
    { "loader/0501_device_cmyk8_separate_builtin", test_loader_0501_device },
    { "loader/0502_device_cmyk8_tile_builtin", test_loader_0502_device },
    { "loader/0503_device_cmyk16_tile_builtin", test_loader_0503_device },
    { "loader/0504_device_lab8_alpha_builtin", test_loader_0504_device },
    { "loader/0505_device_cmyk8_alpha_builtin", test_loader_0505_device },
    { "loader/0506_device_cmyk16_alpha_builtin", test_loader_0506_device },
    { "loader/0507_device_cielab16_rotated_builtin", test_loader_0507_device },
    { "loader/0508_device_cmyk8_opaque_alpha_builtin", test_loader_0508_device },
    { "loader/0509_device_cielab8_strip_lcms2", test_loader_0509_device },
    { "loader/0510_device_cielab16_tile_lcms2", test_loader_0510_device },
    { "loader/0511_device_icclab8_separate_lcms2", test_loader_0511_device },
    { "loader/0512_device_itulab8_tile_lcms2", test_loader_0512_device },
    { "loader/0513_device_cmyk8_strip_lcms2", test_loader_0513_device },
    { "loader/0514_device_cmyk16_tile_lcms2", test_loader_0514_device },
    { "loader/0515_device_cmyk16_alpha_lcms2", test_loader_0515_device },
    { "loader/0516_device_cielab8_strip_colorsync", test_loader_0516_device },
    { "loader/0517_device_cielab16_tile_colorsync", test_loader_0517_device },
    { "loader/0518_device_icclab8_separate_colorsync", test_loader_0518_device },
    { "loader/0519_device_itulab8_tile_colorsync", test_loader_0519_device },
    { "loader/0520_device_cmyk8_strip_colorsync", test_loader_0520_device },
    { "loader/0521_device_cmyk16_tile_colorsync", test_loader_0521_device },
    { "loader/0522_device_cmyk16_alpha_colorsync", test_loader_0522_device },
    { "loader/0523_device_cielab8_strip_none", test_loader_0523_device },
    { "loader/0524_device_cielab16_tile_none", test_loader_0524_device },
    { "loader/0525_device_icclab8_separate_none", test_loader_0525_device },
    { "loader/0526_device_itulab8_tile_none", test_loader_0526_device },
    { "loader/0527_device_cmyk8_strip_none", test_loader_0527_device },
    { "loader/0528_device_cmyk16_tile_none", test_loader_0528_device },
    { "loader/0529_device_cmyk8_alpha_none", test_loader_0529_device },
    { "loader/0530_device_cmyk16_alpha_none", test_loader_0530_device },
    { "loader/0531_device_cmyk8_opaque_alpha_none", test_loader_0531_device },
    { "loader/0532_device_cielab8_strip_absent", test_loader_0532_device },
    { "loader/0533_device_cielab8_strip_invalid", test_loader_0533_device },
    { "loader/0534_device_cielab8_strip_mismatch", test_loader_0534_device },
    { "loader/0535_device_cmyk8_strip_absent", test_loader_0535_device },
    { "loader/0536_device_cmyk8_strip_invalid", test_loader_0536_device },
    { "loader/0537_device_cmyk8_strip_mismatch", test_loader_0537_device },
    { "loader/0538_device_frame_allocation", test_loader_0538_device },
    { "loader/0539_device_row_allocation", test_loader_0539_device },
    { "loader/0540_device_color_allocation", test_loader_0540_device },
    { "loader/0541_device_late_plane", test_loader_0541_device },
    { "loader/0542_device_short_tile", test_loader_0542_device },
    { "loader/0543_device_lab_declined", test_loader_0543_device },
    { "loader/0544_device_cms_allocation", test_loader_0544_device },
    { "loader/0545_device_cmyk_prefix_failure_builtin", test_loader_0545_device },
    { "loader/0546_device_cmyk_nonfinite_result_builtin", test_loader_0546_device },
    { "loader/0547_device_cmyk_prefix_failure_lcms2", test_loader_0547_device },
    { "loader/0548_device_cmyk_nonfinite_result_lcms2", test_loader_0548_device },
    { "loader/0549_device_cmyk_prefix_failure_colorsync", test_loader_0549_device },
    { "loader/0550_device_cmyk_nonfinite_result_colorsync", test_loader_0550_device },
    { "loader/0551_device_lab_target_gamma", test_loader_0551_device },
    { "loader/0552_device_lab_target_byte", test_loader_0552_device },
    { "loader/0553_device_cmyk_target_gamma", test_loader_0553_device },
    { "loader/0554_device_cmyk_target_byte", test_loader_0554_device },
    { "loader/0555_device_lab8_alpha_background", test_loader_0555_device },
    { "loader/0556_device_cmyk8_alpha_background", test_loader_0556_device },
    { "loader/0557_device_cmyk16_alpha_background", test_loader_0557_device },
    { "loader/0491_lab_layout_profile_guard",
      test_loader_0491_lab_layout },
    { "loader/0492_lab8_gamma_target",
      test_loader_0492_lab_layout },
    { "loader/0493_lab8_byte_target",
      test_loader_0493_lab_layout },
    { "loader/0475_icclab8_source",
      test_loader_0475_lab_layout },
    { "loader/0476_itulab8_source",
      test_loader_0476_lab_layout },
    { "loader/0477_itulab8_decode",
      test_loader_0477_lab_layout },
    { "loader/0478_lab16_planar_le_source",
      test_loader_0478_lab_layout },
    { "loader/0479_lab16_planar_be_source",
      test_loader_0479_lab_layout },
    { "loader/0480_lab16_planar_packbits",
      test_loader_0480_lab_layout },
    { "loader/0481_icclab8_unmanaged",
      test_loader_0481_lab_layout },
    { "loader/0482_itulab8_unmanaged",
      test_loader_0482_lab_layout },
    { "loader/0483_itulab8_decode_unmanaged",
      test_loader_0483_lab_layout },
    { "loader/0484_lab16_planar_unmanaged",
      test_loader_0484_lab_layout },
    { "loader/0485_lab_layout_builtin",
      test_loader_0485_lab_layout },
    { "loader/0486_lab_layout_lcms2",
      test_loader_0486_lab_layout },
    { "loader/0487_lab_layout_colorsync",
      test_loader_0487_lab_layout },
    { "loader/0488_itulab8_invalid_decode",
      test_loader_0488_lab_layout },
    { "loader/0489_lab16_planar_late_read",
      test_loader_0489_lab_layout },
    { "loader/0490_lab_layout_allocation",
      test_loader_0490_lab_layout },
    { "loader/0383_separate16_rgb_source",
      test_loader_0383_sep16 },
    { "loader/0384_separate16_black_source",
      test_loader_0384_sep16 },
    { "loader/0385_separate16_white_source",
      test_loader_0385_sep16 },
    { "loader/0386_separate16_orientation3",
      test_loader_0386_sep16 },
    { "loader/0387_separate16_void",
      test_loader_0387_sep16 },
    { "loader/0388_separate16_packbits_order",
      test_loader_0388_sep16 },
    { "loader/0389_separate16_builtin",
      test_loader_0389_sep16 },
    { "loader/0390_separate16_lcms2",
      test_loader_0390_sep16 },
    { "loader/0391_separate16_colorsync",
      test_loader_0391_sep16 },
    { "loader/0392_separate16_rgb_profile_guard",
      test_loader_0392_sep16 },
    { "loader/0393_separate16_gray_profile_guard",
      test_loader_0393_sep16 },
    { "loader/0394_separate16_absent_profile",
      test_loader_0394_sep16 },
    { "loader/0395_separate16_invalid_profile",
      test_loader_0395_sep16 },
    { "loader/0396_separate16_row_allocation",
      test_loader_0396_sep16 },
    { "loader/0397_separate16_source_allocation",
      test_loader_0397_sep16 },
    { "loader/0398_separate16_zero_row_size",
      test_loader_0398_sep16 },
    { "loader/0399_separate16_negative_row_size",
      test_loader_0399_sep16 },
    { "loader/0400_separate16_short_row_size",
      test_loader_0400_sep16 },
    { "loader/0401_separate16_later_row_failure",
      test_loader_0401_sep16 },
    { "loader/0402_separate16_later_plane_failure",
      test_loader_0402_sep16 },
    { "loader/0403_separate16_rgb_cms_allocation",
      test_loader_0403_sep16 },
    { "loader/0404_separate16_gray_pack_allocation",
      test_loader_0404_sep16 },
    { "loader/0405_separate16_gray_cms_allocation",
      test_loader_0405_sep16 },
    { "loader/0406_separate16_target_failure",
      test_loader_0406_sep16 },
    { "loader/0407_separate16_builtin_prefix",
      test_loader_0407_sep16 },
    { "loader/0408_separate16_lcms2_prefix",
      test_loader_0408_sep16 },
    { "loader/0409_separate16_colorsync_prefix",
      test_loader_0409_sep16 },
    { "loader/0410_separate16_nonfinite_result",
      test_loader_0410_sep16 },
    { "loader/0411_separate16_target_policy",
      test_loader_0411_sep16 },
    { "loader/0412_separate16_excluded_layouts",
      test_loader_0412_sep16 },
    { "loader/0186_png_byte_target", test_loader_0186_png_byte_target },
    { "loader/0189_byte_storage", test_loader_0189_byte_storage },
    { "loader/0187_coregraphics_png", test_loader_0187_coregraphics_png },
    { "loader/0188_colorsync_linear", test_loader_0188_colorsync_linear },
    { "loader/0189_png_palette_samples",
      test_loader_0189_png_palette_samples },
    { "loader/0190_png_palette_failure",
      test_loader_0190_png_palette_failure },
    { "loader/0191_png_palette_paint",
      test_loader_0191_png_palette_paint },
    { "loader/0192_png_metadata_samples",
      test_loader_0192_png_metadata_samples },
    { "loader/0193_png_metadata_failures",
      test_loader_0193_png_metadata_failures },
    { "loader/0194_png_metadata_ownership",
      test_loader_0194_png_metadata_ownership },
    { "loader/0195_png_chrm_roundtrip",
      test_loader_0195_png_chrm_roundtrip },
    { "loader/0196_png_chrm_boundary",
      test_loader_0196_png_chrm_boundary },
    { "loader/0197_png_chrm_reject",
      test_loader_0197_png_chrm_reject },
    { "loader/0198_png_chrm_samples",
      test_loader_0198_png_chrm_samples },
    { "loader/0199_png_chrm_getter_reject",
      test_loader_0199_png_chrm_getter_reject },
    { "loader/0200_png_chrm_trace_control",
      test_loader_0200_png_chrm_trace_control },
    { "loader/0201_png_chrm_area", test_loader_0201_png_chrm_area },
    { "loader/0202_png_chrm_accept", test_loader_0202_png_chrm_accept },
    { "loader/0203_png_arith_dispatch", test_loader_0203_png_arith_dispatch },
    { "loader/0204_png_arith_exact", test_loader_0204_png_arith_exact },
    { "loader/0205_png_arith_double", test_loader_0205_png_arith_double },
    { "loader/0206_png_arith_neutral", test_loader_0206_png_arith_neutral },
    { "loader/0207_png_arith_srgb_target",
      test_loader_0207_png_arith_srgb_target },
    { "loader/0208_png_arith_singular", test_loader_0208_png_arith_singular },
    { "loader/0209_png_arith_invalid", test_loader_0209_png_arith_invalid },
    { "loader/0210_png_arith_loader", test_loader_0210_png_arith_loader },
    { "loader/0211_png_arith_unavailable",
      test_loader_0211_png_arith_unavailable },
    { "loader/0212_png_arith_tiny", test_loader_0212_png_arith_tiny },

    { "loader/0020_loader_librsvg_detect_svg_like",
      test_loader_0020_loader_librsvg_detect_svg_like },
    { "loader/0021_loader_builtin_indexed_png_reqcolors_fallback",
      test_loader_0021_loader_builtin_indexed_png_reqcolors_fallback },
    { "loader/0022_loader_libpng_indexed_png_reqcolors_fallback",
      test_loader_0022_loader_libpng_indexed_png_reqcolors_fallback },
    { "loader/0023_loader_librsvg_pixelformat",
      test_loader_0023_loader_librsvg_pixelformat },
    { "loader/0025_loader_librsvg_decode_mode",
      test_loader_0025_loader_librsvg_decode_mode },
    { "loader/0026_loader_librsvg_setopt_compat",
      test_loader_0026_loader_librsvg_setopt_compat },
    { "loader/0032_loader_librsvg_setopt_ignored_diag",
      test_loader_0032_loader_librsvg_setopt_ignored_diag },
    { "loader/0033_loader_librsvg_bgcolor_pixelformat",
      test_loader_0033_loader_librsvg_bgcolor_pixelformat },
#if defined(SIXEL_ENABLE_GDK_PIXBUF_LOADER_TESTS)
    { "gdk-pixbuf-loader/0001_gdk_pixbuf_loader",
      test_gdk_pixbuf_loader_0001_gdk_pixbuf_loader },
    { "gdk-pixbuf-loader/0002_incremental_load",
      test_gdk_pixbuf_loader_0002_incremental_load },
    { "gdk-pixbuf-loader/0003_corrupt_data",
      test_gdk_pixbuf_loader_0003_corrupt_data },
    { "gdk-pixbuf-loader/0004_propagate_error",
      test_gdk_pixbuf_loader_0004_propagate_error },
    { "gdk-pixbuf-loader/0005_context_free",
      test_gdk_pixbuf_loader_0005_context_free },
    { "gdk-pixbuf-loader/0006_loader_module",
      test_gdk_pixbuf_loader_0006_loader_module },
    { "gdk-pixbuf-loader/0007_loader_plugin_integration",
      test_gdk_pixbuf_loader_0007_loader_plugin_integration },
#endif
    { NULL, NULL }
};

static void
print_usage(char const *program)
{
    size_t index;

    fprintf(stderr, "usage: %s <test-name> [args...]\n", program);
    fprintf(stderr, "       %s --list\n", program);
    fprintf(stderr, "       %s --is-running-under-wine\n", program);
    fprintf(stderr,
            "       %s --win32-ctrl-break-run <delay-ms> <timeout-ms|0> "
            "<program> [args...]\n",
            program);
    fprintf(stderr,
            "       %s --sigint-run <delay-ms> <timeout-ms|0> "
            "<program> [args...]\n",
            program);
    fprintf(stderr,
            "       %s --sigint-run-until <needle> <timeout-ms|0> "
            "<program> [args...]\n",
            program);
    fprintf(stderr, "\n");
    fprintf(stderr, "available tests:\n");
    for (index = 0u; test_entries[index].name != NULL; index++) {
        fprintf(stderr, "  %s\n", test_entries[index].name);
    }
}

static int
test_runner_is_running_under_wine(void)
{
#if defined(_WIN32)
    HMODULE ntdll;
    FARPROC wine_get_version;

    ntdll = NULL;
    wine_get_version = NULL;

    ntdll = GetModuleHandleA("ntdll.dll");
    if (ntdll == NULL) {
        return 0;
    }

    wine_get_version = GetProcAddress(ntdll, "wine_get_version");
    if (wine_get_version == NULL) {
        return 0;
    }

    return 1;
#else
    return 0;
#endif
}

#if defined(_WIN32) || defined(__CYGWIN__)
/*
 * CreateProcessA requires native Win32 drive paths.
 * Convert Cygwin/MSYS drive forms so child launch stays stable when
 * this runner is invoked from POSIX-like shells.
 */
static char *
test_runner_duplicate_win32_path(char const *path)
{
    size_t source_length;
    size_t source_index;
    size_t drive_index;
    size_t payload_length;
    size_t target_length;
    size_t index;
    char *converted;
    int convert_drive;

    source_length = 0u;
    source_index = 0u;
    drive_index = 0u;
    payload_length = 0u;
    target_length = 0u;
    index = 0u;
    converted = NULL;
    convert_drive = 0;

    if (path == NULL) {
        return NULL;
    }

    source_length = strlen(path);
    if (source_length >= 11u &&
            strncmp(path, "/cygdrive/", 10u) == 0 &&
            isalpha((unsigned char)path[10]) &&
            (path[11] == '/' || path[11] == '\\' || path[11] == '\0')) {
        drive_index = 10u;
        source_index = 11u;
        convert_drive = 1;
    } else if (source_length >= 2u && path[0] == '/' &&
               isalpha((unsigned char)path[1]) &&
               (path[2] == '/' || path[2] == '\\' || path[2] == '\0')) {
        drive_index = 1u;
        source_index = 2u;
        convert_drive = 1;
    }

    if (!convert_drive) {
        converted = (char *)malloc(source_length + 1u);
        if (converted == NULL) {
            return NULL;
        }
        memcpy(converted, path, source_length + 1u);
        return converted;
    }

    /*
     * Skip the optional slash after the drive token while keeping drive_index
     * anchored to the original letter position.
     */
    if (source_index < source_length &&
            (path[source_index] == '/' || path[source_index] == '\\')) {
        source_index += 1u;
    }

    if (source_index > source_length) {
        return NULL;
    }
    if (drive_index >= source_length
            || !isalpha((unsigned char)path[drive_index])) {
        return NULL;
    }

    /*
     * Keep allocation and copy bounds in the same unit after removing
     * an optional separator so static analyzers can track the limit.
     */
    payload_length = source_length - source_index;
    target_length = payload_length + 4u;
    converted = (char *)malloc(target_length);
    if (converted == NULL) {
        return NULL;
    }

    converted[0] = (char)toupper((unsigned char)path[drive_index]);
    converted[1] = ':';
    converted[2] = '\\';
    index = 3u;

    while (payload_length > 0u) {
        if (index + 1u >= target_length) {
            free(converted);
            return NULL;
        }
        if (path[source_index] == '/') {
            converted[index] = '\\';
        } else {
            converted[index] = path[source_index];
        }
        ++source_index;
        ++index;
        --payload_length;
    }
    if (index >= target_length) {
        free(converted);
        return NULL;
    }
    converted[index] = '\0';

    return converted;
}
#endif

static int
test_runner_run_windows_ctrl_break(int argc, char **argv)
{
#if defined(_WIN32) || defined(__CYGWIN__)
    char const *delay_token;
    char const *timeout_token;
    unsigned long parsed_delay;
    unsigned long parsed_timeout;
    char *delay_end;
    char *timeout_end;
    char **command_tokens;
    size_t command_token_count;
    char const *program;
    char const *token;
    STARTUPINFOA startup_info;
    PROCESS_INFORMATION process_info;
    char *command_line;
    size_t command_line_length;
    size_t index;
    size_t token_index;
    size_t token_length;
    char *cursor;
    char const *token_cursor;
    size_t backslash_count;
    DWORD wait_result;
    DWORD wait_timeout;
    BOOL create_ok;
    BOOL handler_ignore_set;
    BOOL ctrl_break_sent;
    BOOL child_terminated;
    int exit_status;

    delay_token = NULL;
    timeout_token = NULL;
    parsed_delay = 0ul;
    parsed_timeout = 0ul;
    delay_end = NULL;
    timeout_end = NULL;
    command_tokens = NULL;
    command_token_count = 0u;
    program = NULL;
    token = NULL;
    memset(&startup_info, 0, sizeof(startup_info));
    memset(&process_info, 0, sizeof(process_info));
    command_line = NULL;
    command_line_length = 1u;
    index = 0u;
    token_index = 0u;
    token_length = 0u;
    cursor = NULL;
    token_cursor = NULL;
    backslash_count = 0u;
    wait_result = WAIT_FAILED;
    wait_timeout = INFINITE;
    create_ok = FALSE;
    handler_ignore_set = FALSE;
    ctrl_break_sent = FALSE;
    child_terminated = FALSE;
    exit_status = EXIT_FAILURE;

    if (argc < 4) {
        fprintf(stderr,
                "usage: %s <delay-ms> <timeout-ms> <program> [args...]\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    delay_token = argv[1];
    timeout_token = argv[2];
    errno = 0;
    parsed_delay = strtoul(delay_token, &delay_end, 10);
    if (errno != 0 || delay_end == delay_token || *delay_end != '\0') {
        fprintf(stderr,
                "test_runner: invalid delay milliseconds: %s\n",
                delay_token);
        return EXIT_FAILURE;
    }

    errno = 0;
    parsed_timeout = strtoul(timeout_token, &timeout_end, 10);
    if (errno != 0 || timeout_end == timeout_token || *timeout_end != '\0') {
        fprintf(stderr,
                "test_runner: invalid timeout milliseconds: %s\n",
                timeout_token);
        return EXIT_FAILURE;
    }

    if (parsed_delay > (unsigned long)MAXDWORD) {
        fprintf(stderr,
                "test_runner: delay milliseconds exceeds DWORD range\n");
        return EXIT_FAILURE;
    }

    if (parsed_timeout > (unsigned long)MAXDWORD) {
        fprintf(stderr,
                "test_runner: timeout milliseconds exceeds DWORD range\n");
        return EXIT_FAILURE;
    }

    command_token_count = (size_t)argc - 3u;
    command_tokens = (char **)calloc(command_token_count, sizeof(char *));
    if (command_tokens == NULL) {
        fprintf(stderr, "test_runner: failed to allocate token array\n");
        return EXIT_FAILURE;
    }

    for (index = 0u; index < command_token_count; ++index) {
        command_tokens[index] =
            test_runner_duplicate_win32_path(argv[index + 3u]);
        if (command_tokens[index] == NULL) {
            fprintf(stderr,
                    "test_runner: failed to normalize command token\n");
            goto cleanup;
        }
    }
    program = command_tokens[0];

    for (index = 0u; index < command_token_count; ++index) {
        token = command_tokens[index];
        token_length = strlen(token);
        command_line_length += 3u + (token_length * 2u);
    }

    command_line = (char *)malloc(command_line_length);
    if (command_line == NULL) {
        fprintf(stderr, "test_runner: failed to allocate command line\n");
        return EXIT_FAILURE;
    }

    cursor = command_line;
    for (index = 0u; index < command_token_count; ++index) {
        if (index != 0u) {
            *cursor++ = ' ';
        }
        *cursor++ = '"';
        token_cursor = command_tokens[index];
        backslash_count = 0u;
        while (*token_cursor != '\0') {
            if (*token_cursor == '\\') {
                backslash_count++;
                token_cursor++;
                continue;
            }
            if (*token_cursor == '"') {
                while (backslash_count > 0u) {
                    *cursor++ = '\\';
                    *cursor++ = '\\';
                    backslash_count--;
                }
                *cursor++ = '\\';
                *cursor++ = '"';
                token_cursor++;
                continue;
            }
            while (backslash_count > 0u) {
                *cursor++ = '\\';
                backslash_count--;
            }
            *cursor++ = *token_cursor;
            token_cursor++;
        }
        while (backslash_count > 0u) {
            *cursor++ = '\\';
            *cursor++ = '\\';
            backslash_count--;
        }
        *cursor++ = '"';
    }
    *cursor = '\0';

    startup_info.cb = sizeof(startup_info);
    create_ok = CreateProcessA(program,
                               command_line,
                               NULL,
                               NULL,
                               FALSE,
                               CREATE_NEW_PROCESS_GROUP,
                               NULL,
                               NULL,
                               &startup_info,
                               &process_info);
    if (!create_ok) {
        fprintf(stderr,
                "test_runner: CreateProcessA failed: %lu\n",
                (unsigned long)GetLastError());
        goto cleanup;
    }

    if (parsed_delay > 0ul) {
        Sleep((DWORD)parsed_delay);
    }

    handler_ignore_set = SetConsoleCtrlHandler(NULL, TRUE);
    if (!handler_ignore_set) {
        fprintf(stderr,
                "test_runner: SetConsoleCtrlHandler(ignore) failed: %lu\n",
                (unsigned long)GetLastError());
        goto cleanup;
    }

    ctrl_break_sent = GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT,
                                               process_info.dwProcessId);
    if (!ctrl_break_sent) {
        fprintf(stderr,
                "test_runner: GenerateConsoleCtrlEvent failed: %lu\n",
                (unsigned long)GetLastError());
        goto cleanup;
    }

    if (parsed_timeout > 0ul) {
        wait_timeout = (DWORD)parsed_timeout;
    }
    wait_result = WaitForSingleObject(process_info.hProcess,
                                      wait_timeout);
    if (wait_result == WAIT_OBJECT_0) {
        child_terminated = TRUE;
        exit_status = EXIT_SUCCESS;
        goto cleanup;
    }

    if (wait_result == WAIT_TIMEOUT) {
        fprintf(stderr,
                "test_runner: timeout waiting child after CTRL_BREAK_EVENT\n");
        goto cleanup;
    }

    fprintf(stderr,
            "test_runner: WaitForSingleObject failed: %lu\n",
            (unsigned long)GetLastError());

cleanup:
    if (handler_ignore_set) {
        SetConsoleCtrlHandler(NULL, FALSE);
    }
    if (!child_terminated && process_info.hProcess != NULL) {
        TerminateProcess(process_info.hProcess, 1u);
        WaitForSingleObject(process_info.hProcess, 5000u);
    }
    if (process_info.hThread != NULL) {
        CloseHandle(process_info.hThread);
    }
    if (process_info.hProcess != NULL) {
        CloseHandle(process_info.hProcess);
    }
    if (command_tokens != NULL) {
        for (token_index = 0u; token_index < command_token_count;
                ++token_index) {
            free(command_tokens[token_index]);
        }
        free(command_tokens);
    }
    free(command_line);
    return exit_status;
#else
    (void)argc;
    (void)argv;

    fprintf(stderr,
            "test_runner: --win32-ctrl-break-run is unavailable\n");
    return EXIT_FAILURE;
#endif
}

/*
 * The SIGINT-driven runner is available only on non-Windows builds.
 * Keep its sleep helper in the same platform scope so -Wunused-function
 * does not fail Windows toolchains that treat warnings as errors.
 */
#if TEST_RUNNER_HAVE_POSIX_SIGINT_RUNNER
static int
test_runner_sleep_milliseconds(unsigned long milliseconds)
{
    struct timespec delay;
    struct timespec remain;
    int sleep_status;

    delay.tv_sec = 0;
    delay.tv_nsec = 0L;
    remain.tv_sec = 0;
    remain.tv_nsec = 0L;
    sleep_status = 0;

    delay.tv_sec = (time_t)(milliseconds / 1000ul);
    delay.tv_nsec = (long)(milliseconds % 1000ul) * 1000000L;
    for (;;) {
        sleep_status = nanosleep(&delay, &remain);
        if (sleep_status == 0) {
            return 0;
        }
        if (errno != EINTR) {
            return -1;
        }
        delay = remain;
    }
}
#endif

#if TEST_RUNNER_HAVE_POSIX_SIGINT_RUNNER
/*
 * Some runtimes (notably APE wrappers) do not always propagate signals sent
 * only to the process group. Send to both the process group and the direct
 * child so SIGINT/SIGKILL delivery remains stable across runtimes.
 */
static int
test_runner_signal_group_and_child(pid_t child_pid, int signum)
{
    int group_result;
    int child_result;
    int group_errno;
    int child_errno;
    pid_t child_pgid;
    int should_signal_child;

    group_result = 0;
    child_result = 0;
    group_errno = 0;
    child_errno = 0;
    child_pgid = (pid_t)-1;
    should_signal_child = 1;

    group_result = kill((pid_t)(-child_pid), signum);
    if (group_result != 0) {
        group_errno = errno;
    }

    if (group_result == 0) {
        child_pgid = getpgid(child_pid);
        if (child_pgid == child_pid) {
            should_signal_child = 0;
        }
    }

    /*
     * Avoid delivering the same signal twice to the group leader.
     * Keep direct-child fallback when the process-group relationship
     * is unknown so wrapped runtimes still get signal delivery.
     */
    if (should_signal_child != 0) {
        child_result = kill(child_pid, signum);
        if (child_result != 0) {
            child_errno = errno;
        }
    }

    if ((group_errno == 0 || group_errno == ESRCH)
            && (child_errno == 0 || child_errno == ESRCH)) {
        return 0;
    }

    if (child_errno != 0 && child_errno != ESRCH) {
        errno = child_errno;
    } else {
        errno = group_errno;
    }
    return -1;
}
#endif

static int
test_runner_run_posix_sigint(int argc, char **argv)
{
#if TEST_RUNNER_HAVE_POSIX_SIGINT_RUNNER
    char const *delay_token;
    char const *timeout_token;
    unsigned long parsed_delay;
    unsigned long parsed_timeout;
    char *delay_end;
    char *timeout_end;
    char const *program;
    char **program_argv;
    pid_t child_pid;
    pid_t wait_pid;
    int wait_status;
    int child_running;
    unsigned long elapsed_ms;
    unsigned long poll_interval_ms;
    int send_result;
    int kill_result;

    delay_token = NULL;
    timeout_token = NULL;
    parsed_delay = 0ul;
    parsed_timeout = 0ul;
    delay_end = NULL;
    timeout_end = NULL;
    program = NULL;
    program_argv = NULL;
    child_pid = (pid_t)0;
    wait_pid = (pid_t)0;
    wait_status = 0;
    child_running = 1;
    elapsed_ms = 0ul;
    poll_interval_ms = 10ul;
    send_result = 0;
    kill_result = 0;

    if (argc < 4) {
        fprintf(stderr,
                "usage: %s <delay-ms> <timeout-ms> <program> [args...]\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    delay_token = argv[1];
    timeout_token = argv[2];
    program = argv[3];
    program_argv = argv + 3;

    errno = 0;
    parsed_delay = strtoul(delay_token, &delay_end, 10);
    if (errno != 0 || delay_end == delay_token || *delay_end != '\0') {
        fprintf(stderr,
                "test_runner: invalid delay milliseconds: %s\n",
                delay_token);
        return EXIT_FAILURE;
    }

    errno = 0;
    parsed_timeout = strtoul(timeout_token, &timeout_end, 10);
    if (errno != 0 || timeout_end == timeout_token || *timeout_end != '\0') {
        fprintf(stderr,
                "test_runner: invalid timeout milliseconds: %s\n",
                timeout_token);
        return EXIT_FAILURE;
    }

    child_pid = fork();
    if (child_pid < (pid_t)0) {
        fprintf(stderr, "test_runner: fork failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    if (child_pid == (pid_t)0) {
        (void)setpgid(0, 0);
        (void)execvp(program, program_argv);
        fprintf(stderr, "test_runner: execvp failed: %s\n", strerror(errno));
        _exit(127);
    }

    if (setpgid(child_pid, child_pid) != 0
        && errno != EACCES && errno != ESRCH) {
        fprintf(stderr, "test_runner: setpgid failed: %s\n", strerror(errno));
    }

    if (parsed_delay > 0ul
        && test_runner_sleep_milliseconds(parsed_delay) != 0) {
        fprintf(stderr, "test_runner: nanosleep failed: %s\n", strerror(errno));
        goto timeout_cleanup;
    }

    send_result = test_runner_signal_group_and_child(child_pid, SIGINT);
    if (send_result != 0) {
        fprintf(stderr, "test_runner: SIGINT send failed: %s\n",
                strerror(errno));
    }

    if (parsed_timeout > 0ul && poll_interval_ms > parsed_timeout) {
        poll_interval_ms = parsed_timeout;
    }

    while (child_running != 0) {
        wait_pid = waitpid(child_pid, &wait_status, WNOHANG);
        if (wait_pid == child_pid) {
            child_running = 0;
            break;
        }
        if (wait_pid < (pid_t)0) {
            if (errno == EINTR) {
                continue;
            }
            fprintf(stderr, "test_runner: waitpid failed: %s\n",
                    strerror(errno));
            goto timeout_cleanup;
        }
        if (parsed_timeout > 0ul && elapsed_ms >= parsed_timeout) {
            fprintf(stderr,
                    "test_runner: timeout waiting child after SIGINT\n");
            goto timeout_cleanup;
        }
        if (test_runner_sleep_milliseconds(poll_interval_ms) != 0) {
            fprintf(stderr, "test_runner: nanosleep failed: %s\n",
                    strerror(errno));
            goto timeout_cleanup;
        }
        if (parsed_timeout > 0ul) {
            if (elapsed_ms > parsed_timeout - poll_interval_ms) {
                elapsed_ms = parsed_timeout;
            } else {
                elapsed_ms += poll_interval_ms;
            }
        }
    }

    return EXIT_SUCCESS;

timeout_cleanup:
    kill_result = test_runner_signal_group_and_child(child_pid, SIGKILL);
    if (kill_result != 0) {
        fprintf(stderr, "test_runner: SIGKILL send failed: %s\n",
                strerror(errno));
    }
    for (;;) {
        wait_pid = waitpid(child_pid, &wait_status, 0);
        if (wait_pid == child_pid) {
            break;
        }
        if (wait_pid < (pid_t)0 && errno == EINTR) {
            continue;
        }
        if (wait_pid < (pid_t)0 && errno == ECHILD) {
            break;
        }
        if (wait_pid < (pid_t)0) {
            fprintf(stderr, "test_runner: waitpid cleanup failed: %s\n",
                    strerror(errno));
            break;
        }
    }
    return EXIT_FAILURE;
#else
    (void)argc;
    (void)argv;

    fprintf(stderr, "test_runner: --sigint-run is unavailable\n");
    return EXIT_FAILURE;
#endif
}

#if TEST_RUNNER_HAVE_POSIX_SIGINT_RUNNER
static int
test_runner_buffer_contains(char const *buffer,
                            size_t buffer_length,
                            char const *needle,
                            size_t needle_length)
{
    size_t offset;
    size_t index;

    offset = 0u;
    index = 0u;

    if (needle_length == 0u) {
        return 1;
    }
    if (buffer == NULL || needle == NULL) {
        return 0;
    }
    if (buffer_length < needle_length) {
        return 0;
    }

    for (offset = 0u; offset + needle_length <= buffer_length; ++offset) {
        for (index = 0u; index < needle_length; ++index) {
            if (buffer[offset + index] != needle[index]) {
                break;
            }
        }
        if (index == needle_length) {
            return 1;
        }
    }

    return 0;
}
#endif

static int
test_runner_run_posix_sigint_until(int argc, char **argv)
{
#if TEST_RUNNER_HAVE_POSIX_SIGINT_RUNNER
    char const *needle_token;
    char const *timeout_token;
    unsigned long parsed_timeout;
    char *timeout_end;
    char const *program;
    char **program_argv;
    size_t needle_length;
    size_t carry_length;
    size_t combined_length;
    size_t copy_length;
    ssize_t read_size;
    int stderr_pipe[2];
    int stderr_read_open;
    int stderr_write_open;
    int stderr_closed;
    pid_t child_pid;
    pid_t wait_pid;
    int wait_status;
    int child_running;
    unsigned long elapsed_ms;
    unsigned long poll_interval_ms;
    int send_result;
    int kill_result;
    int signal_sent;
    int found_trigger;
    int sync_pipe[2];
    int sync_read_open;
    int sync_write_open;
    int sync_closed;
    int sync_fd_flags;
    int sync_fd_size;
    ssize_t sync_read_size;
    char sync_fd_text[32];
    char sync_byte;
    char read_buffer[1024];
    char *combined_buffer;
    size_t combined_capacity;
    int fd_flags;
    int sleep_status;
    int read_progress;
    int token_matched;
    unsigned long resend_interval_ms;
    unsigned long resend_elapsed_ms;
#if HAVE_SIGNAL_H
    struct sigaction sync_action;
    struct sigaction sync_old_action;
    pid_t parent_pid;
    int sync_action_installed;
    int sync_signal_supported;
    int sync_env_status;
    int sync_pid_size;
    char sync_pid_text[32];
#endif

    needle_token = NULL;
    timeout_token = NULL;
    parsed_timeout = 0ul;
    timeout_end = NULL;
    program = NULL;
    program_argv = NULL;
    needle_length = 0u;
    carry_length = 0u;
    combined_length = 0u;
    copy_length = 0u;
    read_size = 0;
    stderr_pipe[0] = -1;
    stderr_pipe[1] = -1;
    stderr_read_open = 0;
    stderr_write_open = 0;
    stderr_closed = 0;
    child_pid = (pid_t)0;
    wait_pid = (pid_t)0;
    wait_status = 0;
    child_running = 1;
    elapsed_ms = 0ul;
    poll_interval_ms = 10ul;
    send_result = 0;
    kill_result = 0;
    signal_sent = 0;
    found_trigger = 0;
    sync_pipe[0] = -1;
    sync_pipe[1] = -1;
    sync_read_open = 0;
    sync_write_open = 0;
    sync_closed = 0;
    sync_fd_flags = 0;
    sync_fd_size = 0;
    sync_read_size = 0;
    sync_fd_text[0] = '\0';
    sync_byte = '\0';
    combined_buffer = NULL;
    combined_capacity = 0u;
    fd_flags = 0;
    sleep_status = 0;
    read_progress = 0;
    token_matched = 0;
    resend_interval_ms = 200ul;
    resend_elapsed_ms = 0ul;
#if HAVE_SIGNAL_H
    memset(&sync_action, 0, sizeof(sync_action));
    memset(&sync_old_action, 0, sizeof(sync_old_action));
    parent_pid = (pid_t)0;
    sync_action_installed = 0;
    sync_signal_supported = 0;
    sync_env_status = 0;
    sync_pid_size = 0;
    sync_pid_text[0] = '\0';
#endif

    if (argc < 5) {
        fprintf(stderr,
                "usage: %s <needle> <timeout-ms> <program> [args...]\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    needle_token = argv[1];
    timeout_token = argv[2];
    program = argv[3];
    program_argv = argv + 3;
    needle_length = strlen(needle_token);

    if (needle_length == 0u) {
        fprintf(stderr, "test_runner: --sigint-run-until needs a token\n");
        return EXIT_FAILURE;
    }

    errno = 0;
    parsed_timeout = strtoul(timeout_token, &timeout_end, 10);
    if (errno != 0 || timeout_end == timeout_token || *timeout_end != '\0') {
        fprintf(stderr,
                "test_runner: invalid timeout milliseconds: %s\n",
                timeout_token);
        return EXIT_FAILURE;
    }

#if HAVE_SIGNAL_H
    /*
     * Use a direct signal handshake when available so SIGINT trigger timing
     * does not depend on runtime-specific stderr forwarding behavior.
     */
    test_runner_sigint_sync_ready = 0;
    sync_action.sa_handler = test_runner_sigint_sync_handler;
    (void)sigemptyset(&sync_action.sa_mask);
# if defined(SA_RESTART)
    sync_action.sa_flags = SA_RESTART;
# else
    sync_action.sa_flags = 0;
# endif
    if (sigaction(SIGUSR1, &sync_action, &sync_old_action) == 0) {
        sync_action_installed = 1;
        sync_signal_supported = 1;
    }

    if (sync_signal_supported != 0) {
        parent_pid = getpid();
        sync_pid_size = snprintf(sync_pid_text,
                                 sizeof(sync_pid_text),
                                 "%ld",
                                 (long)parent_pid);
        if (sync_pid_size <= 0
                || (size_t)sync_pid_size >= sizeof(sync_pid_text)) {
            sync_signal_supported = 0;
        }
    }

    if (sync_signal_supported != 0) {
        sync_env_status = test_runner_setenv_portable(
            TEST_RUNNER_SIGINT_SYNC_EVENT_ENVVAR,
            TEST_RUNNER_SIGINT_SYNC_EVENT_DISPATCH_START);
        if (sync_env_status != 0) {
            sync_signal_supported = 0;
        }
    }
    if (sync_signal_supported != 0) {
        sync_env_status = test_runner_setenv_portable(
            TEST_RUNNER_SIGINT_SYNC_PID_ENVVAR,
            sync_pid_text);
        if (sync_env_status != 0) {
            sync_signal_supported = 0;
        }
    }
#endif

    combined_capacity = needle_length + sizeof(read_buffer);
    combined_buffer = (char *)malloc(combined_capacity);
    if (combined_buffer == NULL) {
        fprintf(stderr,
                "test_runner: failed to allocate sigint trace buffer\n");
        return EXIT_FAILURE;
    }

    /*
     * Expose a dedicated one-byte handshake pipe so callback-handoff
     * synchronization does not depend on stderr buffering or signal runtime
     * behavior. Child-side writes happen in encoder.c at dispatch-start.
     */
    if (pipe(sync_pipe) != 0) {
        fprintf(stderr, "test_runner: sync pipe failed: %s\n",
                strerror(errno));
        goto timeout_cleanup;
    }
    sync_read_open = 1;
    sync_write_open = 1;

    sync_fd_flags = fcntl(sync_pipe[1], F_GETFD, 0);
    if (sync_fd_flags >= 0) {
        (void)fcntl(sync_pipe[1], F_SETFD, sync_fd_flags & ~FD_CLOEXEC);
    }

    sync_fd_flags = fcntl(sync_pipe[0], F_GETFL, 0);
    if (sync_fd_flags >= 0) {
        (void)fcntl(sync_pipe[0], F_SETFL, sync_fd_flags | O_NONBLOCK);
    }

    sync_fd_size = snprintf(sync_fd_text,
                            sizeof(sync_fd_text),
                            "%d",
                            sync_pipe[1]);
    if (sync_fd_size <= 0
            || (size_t)sync_fd_size >= sizeof(sync_fd_text)) {
        fprintf(stderr,
                "test_runner: failed to encode sigint sync fd\n");
        goto timeout_cleanup;
    }
    if (test_runner_setenv_portable(TEST_RUNNER_SIGINT_SYNC_FD_ENVVAR,
                                    sync_fd_text) != 0) {
        fprintf(stderr,
                "test_runner: failed to export sigint sync fd env\n");
        goto timeout_cleanup;
    }

    if (pipe(stderr_pipe) != 0) {
        fprintf(stderr, "test_runner: pipe failed: %s\n", strerror(errno));
        goto timeout_cleanup;
    }
    stderr_read_open = 1;
    stderr_write_open = 1;

    child_pid = fork();
    if (child_pid < (pid_t)0) {
        fprintf(stderr, "test_runner: fork failed: %s\n", strerror(errno));
        goto timeout_cleanup;
    }

    if (child_pid == (pid_t)0) {
        (void)setpgid(0, 0);
        (void)close(sync_pipe[0]);
        (void)close(stderr_pipe[0]);
        (void)dup2(stderr_pipe[1], STDERR_FILENO);
        (void)close(stderr_pipe[1]);
        (void)execvp(program, program_argv);
        fprintf(stderr, "test_runner: execvp failed: %s\n", strerror(errno));
        _exit(127);
    }

    if (setpgid(child_pid, child_pid) != 0
        && errno != EACCES && errno != ESRCH) {
        fprintf(stderr, "test_runner: setpgid failed: %s\n", strerror(errno));
    }

    (void)close(stderr_pipe[1]);
    stderr_write_open = 0;
    (void)close(sync_pipe[1]);
    sync_write_open = 0;

    fd_flags = fcntl(stderr_pipe[0], F_GETFL, 0);
    if (fd_flags >= 0) {
        (void)fcntl(stderr_pipe[0], F_SETFL, fd_flags | O_NONBLOCK);
    }

    if (parsed_timeout > 0ul && poll_interval_ms > parsed_timeout) {
        poll_interval_ms = parsed_timeout;
    }

    for (;;) {
        read_progress = 0;
        if (signal_sent == 0 && sync_closed == 0) {
            for (;;) {
                sync_read_size = read(sync_pipe[0], &sync_byte, 1u);
                if (sync_read_size > 0) {
                    found_trigger = 1;
                    send_result =
                        test_runner_signal_group_and_child(child_pid, SIGINT);
                    if (send_result != 0) {
                        fprintf(stderr,
                                "test_runner: SIGINT send failed: %s\n",
                                strerror(errno));
                    }
                    signal_sent = 1;
                    /*
                     * Keep independent timeout budgets for trigger detection
                     * and post-SIGINT shutdown so slow trace startup does not
                     * consume the child-exit grace window.
                     */
                    elapsed_ms = 0ul;
                    resend_elapsed_ms = 0ul;
                    break;
                }
                if (sync_read_size == 0) {
                    sync_closed = 1;
                    break;
                }
                if (errno == EINTR) {
                    continue;
                }
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
                }
                fprintf(stderr, "test_runner: sync read failed: %s\n",
                        strerror(errno));
                goto timeout_cleanup;
            }
        }
#if HAVE_SIGNAL_H
        if (signal_sent == 0
                && sync_signal_supported != 0
                && test_runner_sigint_sync_ready != 0) {
            found_trigger = 1;
            send_result = test_runner_signal_group_and_child(child_pid, SIGINT);
            if (send_result != 0) {
                fprintf(stderr, "test_runner: SIGINT send failed: %s\n",
                        strerror(errno));
            }
            signal_sent = 1;
            /*
             * Keep independent timeout budgets for trigger detection
             * and post-SIGINT shutdown so slow trigger setup does not
             * consume the child-exit grace window.
             */
            elapsed_ms = 0ul;
            resend_elapsed_ms = 0ul;
        }
#endif
        wait_pid = waitpid(child_pid, &wait_status, WNOHANG);
        if (wait_pid == child_pid) {
            child_running = 0;
        } else if (wait_pid < (pid_t)0) {
            if (errno == EINTR) {
                /* Retry the loop after interrupted waitpid calls. */
            } else if (errno == ECHILD) {
                /*
                 * Some wrapper runtimes reap direct children eagerly.
                 * Treat this the same as observing normal child exit.
                 */
                child_running = 0;
            } else {
                fprintf(stderr, "test_runner: waitpid failed: %s\n",
                        strerror(errno));
                goto timeout_cleanup;
            }
        }

        for (;;) {
            read_size = read(stderr_pipe[0],
                             read_buffer,
                             sizeof(read_buffer));
            if (read_size > 0) {
                read_progress = 1;
                if (fwrite(read_buffer,
                           1u,
                           (size_t)read_size,
                           stderr) != (size_t)read_size) {
                    fprintf(stderr, "test_runner: stderr relay failed\n");
                    goto timeout_cleanup;
                }
                fflush(stderr);

                memcpy(combined_buffer + carry_length,
                       read_buffer,
                       (size_t)read_size);
                combined_length = carry_length + (size_t)read_size;
                token_matched = test_runner_buffer_contains(
                    combined_buffer,
                    combined_length,
                    needle_token,
                    needle_length);
                if (token_matched != 0) {
                    found_trigger = 1;
                    /*
                     * Send the first SIGINT at token detection. Subsequent
                     * retries are rate-limited in the main loop to avoid
                     * per-frame signal storms on looped animations.
                     */
                    if (signal_sent == 0) {
                        send_result = test_runner_signal_group_and_child(
                            child_pid,
                            SIGINT);
                        if (send_result != 0) {
                            fprintf(stderr,
                                    "test_runner: SIGINT send failed: %s\n",
                                    strerror(errno));
                        }
                        signal_sent = 1;
                        /*
                         * Keep independent timeout budgets for trigger
                         * detection and post-SIGINT shutdown so slow trace
                         * startup does not consume the child-exit grace
                         * window.
                         */
                        elapsed_ms = 0ul;
                        resend_elapsed_ms = 0ul;
                    }
                }

                if (needle_length > 1u) {
                    if (combined_length >= needle_length - 1u) {
                        copy_length = needle_length - 1u;
                        memmove(combined_buffer,
                                combined_buffer + combined_length
                                - copy_length,
                                copy_length);
                        carry_length = copy_length;
                    } else {
                        carry_length = combined_length;
                    }
                } else {
                    carry_length = 0u;
                }
                continue;
            }
            if (read_size == 0) {
                stderr_closed = 1;
                break;
            }
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            fprintf(stderr, "test_runner: read failed: %s\n", strerror(errno));
            goto timeout_cleanup;
        }

        if (signal_sent == 0 && child_running == 0) {
            fprintf(stderr,
                    "test_runner: child exited before SIGINT trigger\n");
            goto timeout_cleanup;
        }
        if (signal_sent != 0 && child_running == 0) {
            /*
             * Drain relayed stderr while data is still flowing, but stop
             * waiting once the direct child has exited and no more bytes are
             * immediately readable. Some wrappers keep inherited write ends
             * open in helper descendants, so waiting only for EOF can stall.
             */
            if (stderr_closed != 0 || read_progress == 0) {
                break;
            }
            continue;
        }
        if (signal_sent != 0
                && child_running != 0
                && resend_elapsed_ms >= resend_interval_ms) {
            /*
             * Keep nudging wrapped runtimes that can drop the first SIGINT
             * forwarded from the parent process tree.
             */
            send_result = test_runner_signal_group_and_child(child_pid, SIGINT);
            if (send_result != 0) {
                fprintf(stderr, "test_runner: SIGINT send failed: %s\n",
                        strerror(errno));
            }
            resend_elapsed_ms = 0ul;
        }
        if (parsed_timeout > 0ul && elapsed_ms >= parsed_timeout) {
            if (found_trigger == 0) {
                fprintf(stderr,
                        "test_runner: timeout waiting for trigger token\n");
            } else {
                fprintf(stderr,
                        "test_runner: timeout waiting child after SIGINT\n");
            }
            goto timeout_cleanup;
        }

        sleep_status = test_runner_sleep_milliseconds(poll_interval_ms);
        if (sleep_status != 0) {
            fprintf(stderr, "test_runner: nanosleep failed: %s\n",
                    strerror(errno));
            goto timeout_cleanup;
        }
        if (parsed_timeout > 0ul) {
            if (elapsed_ms > parsed_timeout - poll_interval_ms) {
                elapsed_ms = parsed_timeout;
            } else {
                elapsed_ms += poll_interval_ms;
            }
        }
        if (signal_sent != 0 && child_running != 0) {
            if (resend_elapsed_ms > resend_interval_ms - poll_interval_ms) {
                resend_elapsed_ms = resend_interval_ms;
            } else {
                resend_elapsed_ms += poll_interval_ms;
            }
        }
    }

    if (stderr_read_open != 0) {
        (void)close(stderr_pipe[0]);
    }
    if (sync_read_open != 0) {
        (void)close(sync_pipe[0]);
    }
#if HAVE_SIGNAL_H
    if (sync_action_installed != 0) {
        (void)sigaction(SIGUSR1, &sync_old_action, NULL);
    }
#endif
    free(combined_buffer);
    return EXIT_SUCCESS;

timeout_cleanup:
    if (child_pid > (pid_t)0) {
        kill_result = test_runner_signal_group_and_child(child_pid, SIGKILL);
        if (kill_result != 0) {
            fprintf(stderr, "test_runner: SIGKILL send failed: %s\n",
                    strerror(errno));
        }
        for (;;) {
            wait_pid = waitpid(child_pid, &wait_status, 0);
            if (wait_pid == child_pid) {
                break;
            }
            if (wait_pid < (pid_t)0 && errno == EINTR) {
                continue;
            }
            if (wait_pid < (pid_t)0 && errno == ECHILD) {
                break;
            }
            if (wait_pid < (pid_t)0) {
                fprintf(stderr, "test_runner: waitpid cleanup failed: %s\n",
                        strerror(errno));
                break;
            }
        }
    }
    if (stderr_read_open != 0) {
        (void)close(stderr_pipe[0]);
    }
    if (stderr_write_open != 0) {
        (void)close(stderr_pipe[1]);
    }
    if (sync_read_open != 0) {
        (void)close(sync_pipe[0]);
    }
    if (sync_write_open != 0) {
        (void)close(sync_pipe[1]);
    }
#if HAVE_SIGNAL_H
    if (sync_action_installed != 0) {
        (void)sigaction(SIGUSR1, &sync_old_action, NULL);
    }
#endif
    free(combined_buffer);
    return EXIT_FAILURE;
#else
    (void)argc;
    (void)argv;

    fprintf(stderr, "test_runner: --sigint-run-until is unavailable\n");
    return EXIT_FAILURE;
#endif
}

static int
test_runner_apply_env_assignment(char const *assignment);

static int
test_runner_apply_env_options(int argc, char **argv, int *out_first_index)
{
    int index;
    char const *token;
    char const *assignment;
    int status;

    index = 1;
    token = NULL;
    assignment = NULL;

    while (index < argc) {
        token = argv[index];
        if (strcmp(token, "-%") == 0) {
            if (index + 1 >= argc) {
                fprintf(stderr,
                        "test_runner: short env option requires KEY=VALUE"
                        " argument\n");
                return -1;
            }
            assignment = argv[index + 1];
            status = test_runner_apply_env_assignment(assignment);
            if (status != 0) {
                fprintf(stderr, "test_runner: failed to set env\n");
                return -1;
            }
            index += 2;
            continue;
        }
        if (strncmp(token, "--env=", 6) == 0) {
            assignment = token + 6;
            status = test_runner_apply_env_assignment(assignment);
            if (status != 0) {
                fprintf(stderr, "test_runner: failed to set env\n");
                return -1;
            }
            index += 1;
            continue;
        }
        if (strcmp(token, "--env") == 0) {
            if (index + 1 >= argc) {
                fprintf(stderr,
                        "test_runner: --env requires KEY=VALUE argument\n");
                return -1;
            }
            assignment = argv[index + 1];
            status = test_runner_apply_env_assignment(assignment);
            if (status != 0) {
                fprintf(stderr, "test_runner: failed to set env\n");
                return -1;
            }
            index += 2;
            continue;
        }
        break;
    }

    *out_first_index = index;
    return 0;
}

static int
test_runner_apply_env_assignment(char const *assignment)
{
    char const *separator;
    size_t name_length;
    size_t value_length;
    char *name;
    char *value;
    int status;

    separator = NULL;
    name_length = 0u;
    value_length = 0u;
    name = NULL;
    value = NULL;
    status = -1;

    separator = strchr(assignment, '=');
    if (separator == NULL || separator == assignment) {
        fprintf(stderr, "test_runner: invalid env assignment\n");
        return -1;
    }

    name_length = (size_t)(separator - assignment);
    value_length = strlen(separator + 1);

    /*
     * Keep test-runner option parsing consistent with img2sixel --env
     * behavior even when hosted environments expose a very long PATH.
     */
    name = (char *)malloc(name_length + 1u);
    if (name == NULL) {
        fprintf(stderr, "test_runner: failed to allocate env key\n");
        return -1;
    }
    value = (char *)malloc(value_length + 1u);
    if (value == NULL) {
        free(name);
        fprintf(stderr, "test_runner: failed to allocate env value\n");
        return -1;
    }

    memcpy(name, assignment, name_length);
    name[name_length] = '\0';
    memcpy(value, separator + 1, value_length + 1u);

    status = test_runner_setenv_portable(name, value);
    free(value);
    free(name);
    return status;
}

static int
test_runner_setenv_portable(char const *name, char const *value)
{
#if defined(_MSC_VER)
    if (name == NULL || value == NULL) {
        return -1;
    }
    if (SetEnvironmentVariableA(name, value) == 0) {
        return -1;
    }
    return 0;
#elif defined(_WIN32) && defined(HAVE__PUTENV_S) && HAVE__PUTENV_S
    if (name == NULL || value == NULL) {
        return -1;
    }
    if (_putenv_s(name, value) != 0) {
        return -1;
    }
    return 0;
#else
# if defined(HAVE_SETENV)
    extern int setenv(char const *name, char const *value, int overwrite);
# endif
    if (name == NULL || value == NULL) {
        return -1;
    }
# if defined(HAVE_SETENV)
    return setenv(name, value, 1);
# else
    (void)name;
    (void)value;

    return -1;
# endif
#endif
}

static int
test_runner_process_exit_status(int status)
{
#if defined(LIBSIXEL_OPENVMS)
    extern void __posix_exit(int);

    /*
     * Preserve the POSIX result, including skip (77), at the process boundary.
     * Mapping every nonzero result to one native failure loses that contract.
     * Call the RTL entry point explicitly: in an amalgamated build stdlib.h
     * may already have been included before an _POSIX_EXIT macro could remap
     * exit(). GNV's default header does not declare this entry point.
     */
    __posix_exit(status);
#endif

    return status;
}

int
main(int argc, char **argv)
{
    size_t index;
    char const *requested;
    int first_index;

    index = 0u;
    requested = NULL;
    first_index = 1;

    if (test_runner_apply_env_options(argc, argv, &first_index) != 0) {
        print_usage(argv[0]);
        return test_runner_process_exit_status(EXIT_FAILURE);
    }

    if (argc <= first_index) {
        print_usage(argv[0]);
        return test_runner_process_exit_status(EXIT_FAILURE);
    }

    if (strcmp(argv[first_index], "--list") == 0) {
        for (index = 0u; test_entries[index].name != NULL; index++) {
            printf("%s\n", test_entries[index].name);
        }
        return test_runner_process_exit_status(EXIT_SUCCESS);
    }

    if (strcmp(argv[first_index], "--is-running-under-wine") == 0) {
        return test_runner_process_exit_status(
            test_runner_is_running_under_wine() ? EXIT_SUCCESS
                                                : EXIT_FAILURE);
    }

    if (strcmp(argv[first_index], "--win32-ctrl-break-run") == 0) {
        return test_runner_process_exit_status(
            test_runner_run_windows_ctrl_break(argc - first_index,
                                               argv + first_index));
    }

    if (strcmp(argv[first_index], "--sigint-run") == 0) {
        return test_runner_process_exit_status(
            test_runner_run_posix_sigint(argc - first_index,
                                         argv + first_index));
    }
    if (strcmp(argv[first_index], "--sigint-run-until") == 0) {
        return test_runner_process_exit_status(
            test_runner_run_posix_sigint_until(argc - first_index,
                                               argv + first_index));
    }

    requested = argv[first_index];
    for (index = 0u; test_entries[index].name != NULL; index++) {
        if (strcmp(requested, test_entries[index].name) == 0) {
            return test_runner_process_exit_status(
                test_entries[index].run(argc - first_index,
                                        argv + first_index));
        }
    }

    fprintf(stderr, "unknown test: %s\n", requested);
    print_usage(argv[0]);
    return test_runner_process_exit_status(EXIT_FAILURE);
}
