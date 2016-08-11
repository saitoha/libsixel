#define PERL_NO_GET_CONTEXT
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include <sixel.h>

MODULE = Image::LibSIXEL PACKAGE = Image::LibSIXEL::Encoder PREFIX = encoder_

SV *
encoder_new(void)
  INIT:
    SIXELSTATUS     status;
    sixel_encoder_t *encoder;
    SV              *sv;
    const char      *klass;
  CODE:
    status = sixel_encoder_new(&encoder, NULL);
    if (SIXEL_FAILED(status)) {
      croak("libsixel error: %s", sixel_helper_format_error(status));
      XSRETURN_UNDEF;
    }
    klass = SvPV_nolen(ST(0));
    sv = newSViv(PTR2IV(encoder));
    if (!sv) {
      XSRETURN_UNDEF;
    }
    sv = newRV_noinc(sv);
    sv_bless(sv, gv_stashpv(klass, 1));
    SvREADONLY_on(sv);
    RETVAL = sv;
  OUTPUT:
    RETVAL


void
encoder_setopt(...)
  INIT:
    SIXELSTATUS     status;
    IV              iv;
    sixel_encoder_t *encoder;
    char const *arg;
    char const *optarg = NULL;
  PPCODE:
    if (items != 2 && items != 3) {
        croak("Bad argument count: %d", items);
    }
    iv = SvROK(ST(0)) ? SvIV(SvRV(ST(0))) : SvIV(ST(0));
    encoder = INT2PTR(sixel_encoder_t *, iv);
    arg = SvPV_nolen(ST(1));
    if (sv_len(ST(1)) != 1) {
        croak("Bad argument: %s", arg);
    }
    if (items == 3) {
      optarg = SvPV_nolen(ST(2));
    }
    status = sixel_encoder_setopt(encoder, *arg, optarg);
    if (SIXEL_FAILED(status)) {
        croak("libsixel error: %s", sixel_helper_format_error(status));
    }


void
encoder_encode(...)
  INIT:
    SIXELSTATUS     status;
    IV              iv;
    sixel_encoder_t *encoder;
    char const *infile;
  PPCODE:
    if (items != 2) {
        croak("Bad argument count: %d", items);
    }
    iv = SvROK(ST(0)) ? SvIV(SvRV(ST(0))) : SvIV(ST(0));
    encoder = INT2PTR(sixel_encoder_t *, iv);
    infile = SvPV_nolen(ST(1));
    status = sixel_encoder_encode(encoder, infile);
    if (SIXEL_FAILED(status)) {
        croak("libsixel error: %s", sixel_helper_format_error(status));
    }


void
encoder_DESTROY(...)
  INIT:
    IV              iv;
    sixel_encoder_t *encoder;
  PPCODE:
    iv = SvROK(ST(0)) ? SvIV(SvRV(ST(0))) : SvIV(ST(0));
    encoder = INT2PTR(sixel_encoder_t *, iv);
    sixel_encoder_unref(encoder);



MODULE = Image::LibSIXEL PACKAGE = Image::LibSIXEL::Decoder PREFIX = decoder_

SV *
decoder_new(void)
  INIT:
    SIXELSTATUS     status;
    sixel_decoder_t *decoder;
    SV              *sv;
    const char      *klass;
  CODE:
    status = sixel_decoder_new(&decoder, NULL);
    if (SIXEL_FAILED(status)) {
      croak("libsixel error: %s", sixel_helper_format_error(status));
      XSRETURN_UNDEF;
    }
    klass = SvPV_nolen(ST(0));
    sv = newSViv(PTR2IV(decoder));
    if (!sv) {
      XSRETURN_UNDEF;
    }
    sv = newRV_noinc(sv);
    sv_bless(sv, gv_stashpv(klass, 1));
    SvREADONLY_on(sv);
    RETVAL = sv;
  OUTPUT:
    RETVAL


void
decoder_setopt(...)
  INIT:
    SIXELSTATUS     status;
    IV              iv;
    sixel_decoder_t *decoder;
    char const      *arg;
    char const      *optarg = NULL;
  PPCODE:
    if (items != 2 && items != 3) {
      croak("Bad argument count: %d", items);
    }
    iv = SvROK(ST(0)) ? SvIV(SvRV(ST(0))) : SvIV(ST(0));
    decoder = INT2PTR(sixel_decoder_t *, iv);
    arg = SvPV_nolen(ST(1));
    if (sv_len(ST(1)) != 1) {
      croak("Bad argument: %s", arg);
    }
    if (items == 3) {
      optarg = SvPV_nolen(ST(2));
    }
    status = sixel_decoder_setopt(decoder, *arg, optarg);
    if (SIXEL_FAILED(status)) {
      croak("libsixel error: %s", sixel_helper_format_error(status));
    }


void
decoder_decode(...)
  INIT:
    SIXELSTATUS     status;
    IV              iv;
    sixel_decoder_t *decoder;
  PPCODE:
    if (items != 1) {
      croak("Bad argument count: %d", items);
    }
    iv = SvROK(ST(0)) ? SvIV(SvRV(ST(0))) : SvIV(ST(0));
    decoder = INT2PTR(sixel_decoder_t *, iv);
    status = sixel_decoder_decode(decoder);
    if (SIXEL_FAILED(status)) {
      croak("libsixel error: %s", sixel_helper_format_error(status));
    }


void
decoder_DESTROY(...)
  INIT:
    IV              iv;
    sixel_decoder_t *decoder;
  PPCODE:
    iv = SvROK(ST(0)) ? SvIV(SvRV(ST(0))) : SvIV(ST(0));
    decoder = INT2PTR(sixel_decoder_t *, iv);
    sixel_decoder_unref(decoder);
