/*
 * libsixel Ruby bindings
 *
 * Copyright (c) 2014,2015 Hayaki Saito
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.*
 */

#include <ruby.h>
#include <sixel.h>

static VALUE
get_version(VALUE self)
{
    return rb_str_new2(LIBSIXEL_VERSION);
}


static void
sixel_ruby_encoder_free(sixel_encoder_t *encoder)
{
    sixel_encoder_unref(encoder);
}


static VALUE
sixel_ruby_encoder_alloc(VALUE klass)
{
    sixel_encoder_t *encoder;
    SIXELSTATUS status;

    status = sixel_encoder_new(&encoder, NULL);
    if (SIXEL_FAILED(status)) {
        rb_raise(rb_eRuntimeError,
                 "sixel_encoder_new() failed: %s / %s",
                 sixel_helper_format_error(status),
                 sixel_helper_get_additional_message());
    }

    return Data_Wrap_Struct(klass, NULL, sixel_ruby_encoder_free, encoder);
}


static VALUE
sixel_ruby_encoder_initialize(VALUE self)
{
    return Qnil;
}


static VALUE
sixel_ruby_encoder_setopt(VALUE self, VALUE option, VALUE optval)
{
    sixel_encoder_t *encoder;
    SIXELSTATUS status;

    Data_Get_Struct(self, sixel_encoder_t, encoder);

    status = sixel_encoder_setopt(encoder,
                                  *StringValueCStr(option),
                                  StringValueCStr(optval));
    if (SIXEL_FAILED(status)) {
        rb_raise(rb_eRuntimeError,
                 "sixel_encoder_setopt() failed: %s / %s",
                 sixel_helper_format_error(status),
                 sixel_helper_get_additional_message());
    }

    return Qnil;
}


static VALUE
sixel_ruby_encoder_encode(VALUE self, VALUE filename)
{
    sixel_encoder_t *encoder;
    SIXELSTATUS status;

    Data_Get_Struct(self, sixel_encoder_t, encoder);

    status = sixel_encoder_encode(encoder, StringValueCStr(filename));
    if (SIXEL_FAILED(status)) {
        rb_raise(rb_eRuntimeError,
                 "sixel_encoder_encode() failed: %s / %s",
                 sixel_helper_format_error(status),
                 sixel_helper_get_additional_message());
    }

    return Qnil;
}


static void
sixel_ruby_decoder_free(sixel_decoder_t *decoder)
{
    sixel_decoder_decode(decoder);
}


static VALUE
sixel_ruby_decoder_alloc(VALUE klass)
{
    sixel_decoder_t *decoder;
    SIXELSTATUS status;

    status = sixel_decoder_new(&decoder, NULL);
    if (SIXEL_FAILED(status)) {
        rb_raise(rb_eRuntimeError,
                 "sixel_encoder_encode() failed: %s / %s",
                 sixel_helper_format_error(status),
                 sixel_helper_get_additional_message());
    }

    return Data_Wrap_Struct(klass, NULL, sixel_ruby_decoder_free, decoder);
}


static VALUE
sixel_ruby_decoder_initialize(VALUE self)
{
    return Qnil;
}


static VALUE
sixel_ruby_decoder_setopt(VALUE self, VALUE option, VALUE optval)
{
    sixel_decoder_t *decoder;
    SIXELSTATUS status;

    Data_Get_Struct(self, sixel_decoder_t, decoder);

    status = sixel_decoder_setopt(decoder,
                                  *StringValueCStr(option),
                                  StringValueCStr(optval));
    if (SIXEL_FAILED(status)) {
        rb_raise(rb_eRuntimeError,
                 "sixel_decoder_setopt() failed: %s / %s",
                 sixel_helper_format_error(status),
                 sixel_helper_get_additional_message());
    }

    return Qnil;
}


static VALUE
sixel_ruby_decoder_decode(VALUE self)
{
    sixel_decoder_t *decoder;
    SIXELSTATUS status;

    Data_Get_Struct(self, sixel_decoder_t, decoder);

    status = sixel_decoder_decode(decoder);
    if (SIXEL_FAILED(status)) {
        rb_raise(rb_eRuntimeError,
                 "sixel_decoder_decode() failed: %s / %s",
                 sixel_helper_format_error(status),
                 sixel_helper_get_additional_message());
    }

    return Qnil;
}


void
Init_libsixel()
{
    VALUE mSixel = rb_define_module("Libsixel");

    rb_define_singleton_method(mSixel, "version", get_version, 0);

    VALUE encoder_class = rb_define_class("Encoder", rb_cObject);
    rb_define_alloc_func(encoder_class, sixel_ruby_encoder_alloc);
    rb_define_method(encoder_class, "initialize", sixel_ruby_encoder_initialize, 0);
    rb_define_method(encoder_class, "setopt", sixel_ruby_encoder_setopt, 2);
    rb_define_method(encoder_class, "encode", sixel_ruby_encoder_encode, 1);

    VALUE decoder_class = rb_define_class("Decoder", rb_cObject);
    rb_define_method(decoder_class, "initialize", sixel_ruby_decoder_initialize, 0);
    rb_define_method(decoder_class, "setopt", sixel_ruby_decoder_setopt, 2);
    rb_define_method(decoder_class, "encode", sixel_ruby_decoder_decode, 0);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
