#!/usr/bin/env ruby
#
# Copyright (c) 2025 libsixel developers. See `AUTHORS`.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy of
# this software and associated documentation files (the "Software"), to deal in
# the Software without restriction, including without limitation the rights to
# use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
# the Software, and to permit persons to whom the Software is furnished to do so,
# subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
# FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
# COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
# IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#
require "fiddle"
require "fiddle/import"

module Libsixel
  module API
    extend Fiddle::Importer

    # Search and load libsixel shared library
    LIB_CANDIDATES = [
      "sixel", "libsixel", "sixel-1", "libsixel-1", "msys-sixel", "cygsixel",
      # macOS explicit filename fallback
      "libsixel.dylib"
    ]

    loaded = false
    LIB_CANDIDATES.each do |name|
      begin
        dlload name
        loaded = true
        break
      rescue Fiddle::DLError
        next
      end
    end
    raise LoadError, "libsixel shared library not found" unless loaded

    # Basic constants and status helpers
    SIXEL_FALSE = 0x1000 unless const_defined?(:SIXEL_FALSE)
    # DRCS option layout quick map:
    #   MMV : CHARSET : PATH
    #     |      |        +-> optional tile sink ("-" keeps stdout,
    #     |      |            blank discards tiles)
    #     |      +-> charset number (range depends on MMV,
    #     |            defaults to 1)
    #     +-> mapping revision (0..2, defaults to 2)
    SIXEL_OPTFLAG_DRCS = '@' unless const_defined?(:SIXEL_OPTFLAG_DRCS)
    SIXEL_OPTFLAG_ORMODE = 'O' unless const_defined?(:SIXEL_OPTFLAG_ORMODE)
    # Decoder toggles re-use several single-letter switches that already
    # exist on the encoder side.  The table keeps the double booking
    # obvious for callers mapping CLI flags into binding constants:
    #
    #   +-------+----------------------+---------------------------+
    #   | flag  | decoder meaning      | encoder meaning           |
    #   +-------+----------------------+---------------------------+
    #   |  -d   | palette dequantizer  | diffusion selector        |
    #   |  -D   | RGBA direct output   | pipe-mode (deprecated)    |
    #   +-------+----------------------+---------------------------+
    SIXEL_OPTFLAG_DEQUANTIZE = 'd' unless const_defined?(:SIXEL_OPTFLAG_DEQUANTIZE)
    SIXEL_OPTFLAG_DIRECT = 'D' unless const_defined?(:SIXEL_OPTFLAG_DIRECT)
    # The LUT flag mirrors img2sixel's -L switch.  ASCII flow:
    #
    #       auto
    #        |
    #   +----+----+---------+
    #   |         |
    # classic  certified
    # (5/6bit) (certlut)
    #
    SIXEL_OPTFLAG_HAS_GRI_ARG_LIMIT = 'R' unless const_defined?(:SIXEL_OPTFLAG_HAS_GRI_ARG_LIMIT)
    SIXEL_OPTFLAG_6REVERSIBLE = '6' unless const_defined?(:SIXEL_OPTFLAG_6REVERSIBLE)
    SIXEL_OPTFLAG_LUT_POLICY = 'L' unless const_defined?(:SIXEL_OPTFLAG_LUT_POLICY)
    SIXEL_OPTFLAG_DIFFUSION_CARRY = 'Y' unless const_defined?(:SIXEL_OPTFLAG_DIFFUSION_CARRY)
    SIXEL_OPTFLAG_WORKING_COLORSPACE = 'W' unless const_defined?(:SIXEL_OPTFLAG_WORKING_COLORSPACE)
    SIXEL_OPTFLAG_OUTPUT_COLORSPACE = 'U' unless const_defined?(:SIXEL_OPTFLAG_OUTPUT_COLORSPACE)
    SIXEL_OPTFLAG_LOADERS = 'j' unless const_defined?(:SIXEL_OPTFLAG_LOADERS)
    SIXEL_OPTFLAG_MAPFILE_OUTPUT = 'M' unless const_defined?(:SIXEL_OPTFLAG_MAPFILE_OUTPUT)
    SIXEL_OPTFLAG_QUANTIZE_MODEL = 'Q' unless const_defined?(:SIXEL_OPTFLAG_QUANTIZE_MODEL)
    def self.succeeded?(status) ((status & SIXEL_FALSE) == 0) end
    def self.failed?(status)    ((status & SIXEL_FALSE) != 0) end

    # Callback prototype for sixel_output_new: int (*)(char*, int, void*)
    WriteCallback = Fiddle::Closure::BlockCaller.new(
      Fiddle::TYPE_INT,
      [Fiddle::TYPE_VOIDP, Fiddle::TYPE_INT, Fiddle::TYPE_VOIDP]
    ) { |_data, _size, _priv|
      # Define concrete callbacks at call sites. This is only a type sample.
      0
    }

    # C API coverage from include/sixel.h
    # All pointer-ish arguments are normalized to void*; function pointers too.

    extern "int sixel_allocator_new(void *, void *, void *, void *, void *)"
    extern "void sixel_allocator_ref(void *)"
    extern "void sixel_allocator_unref(void *)"
    extern "void * sixel_allocator_malloc(void *, size_t)"
    extern "void * sixel_allocator_calloc(void *, size_t, size_t)"
    extern "void * sixel_allocator_realloc(void *, void *, size_t)"
    extern "void sixel_allocator_free(void *, void *)"

    extern "int sixel_output_new(void *, void *, void *, void *)"
    extern "void * sixel_output_create(void *, void *)"
    extern "void sixel_output_destroy(void *)"
    extern "void sixel_output_ref(void *)"
    extern "void sixel_output_unref(void *)"
    extern "int sixel_output_get_8bit_availability(void *)"
    extern "void sixel_output_set_8bit_availability(void *, int)"
    extern "void sixel_output_set_gri_arg_limit(void *, int)"
    extern "void sixel_output_set_penetrate_multiplexer(void *, int)"
    extern "void sixel_output_set_skip_dcs_envelope(void *, int)"
    extern "void sixel_output_set_skip_header(void *, int)"
    extern "void sixel_output_set_palette_type(void *, int)"
    extern "void sixel_output_set_ormode(void *, int)"
    extern "void sixel_output_set_encode_policy(void *, int)"

    extern "int sixel_dither_new(void *, int, void *)"
    extern "void * sixel_dither_create(int)"
    extern "void * sixel_dither_get(int)"
    extern "void sixel_dither_destroy(void *)"
    extern "void sixel_dither_ref(void *)"
    extern "void sixel_dither_unref(void *)"
    extern "int sixel_dither_initialize(void *, char *, int, int, int, int, int, int)"
    extern "void sixel_dither_set_diffusion_type(void *, int)"
    extern "void sixel_dither_set_diffusion_scan(void *, int)"
    extern "void sixel_dither_set_diffusion_carry(void *, int)"
    extern "int sixel_dither_get_num_of_palette_colors(void *)"
    extern "int sixel_dither_get_num_of_histogram_colors(void *)"
    extern "int sixel_dither_get_num_of_histgram_colors(void *)"
    extern "char * sixel_dither_get_palette(void *)"
    extern "void sixel_dither_set_palette(void *, char *)"
    extern "void sixel_dither_set_complexion_score(void *, int)"
    extern "void sixel_dither_set_body_only(void *, int)"
    extern "void sixel_dither_set_optimize_palette(void *, int)"
    extern "void sixel_dither_set_pixelformat(void *, int)"
    extern "void sixel_dither_set_transparent(void *, int)"

    extern "int sixel_encode(char *, int, int, int, void *, void *)"
    extern "int sixel_decode_raw(char *, int, char *, int, int, char *, int, void *)"
    extern "int sixel_decode(char *, int, char *, int, int, char *, int, void *)"
    extern "int sixel_decode_direct(char *, int, void *, void *, void *, void *)"

    extern "void sixel_set_threads(int)"

    extern "void sixel_helper_set_additional_message(char *)"
    extern "char * sixel_helper_get_additional_message(void)"
    extern "char * sixel_helper_format_error(int)"
    extern "int sixel_helper_compute_depth(int)"
    extern "int sixel_helper_normalize_pixelformat(char *, int, char *, int, int, int)"
    extern "int sixel_helper_scale_image(char *, char *, int, int, int, int, int, int, void *)"

    extern "int sixel_frame_new(void *, void *)"
    extern "void * sixel_frame_create(void)"
    extern "void sixel_frame_ref(void *)"
    extern "void sixel_frame_unref(void *)"
    extern "int sixel_frame_init(void *, char *, int, int, int, char *, int)"
    extern "char * sixel_frame_get_pixels(void *)"
    extern "char * sixel_frame_get_palette(void *)"
    extern "int sixel_frame_get_width(void *)"
    extern "int sixel_frame_get_height(void *)"
    extern "int sixel_frame_get_ncolors(void *)"
    extern "int sixel_frame_get_pixelformat(void *)"
    extern "int sixel_frame_get_transparent(void *)"
    extern "int sixel_frame_get_multiframe(void *)"
    extern "int sixel_frame_get_delay(void *)"
    extern "int sixel_frame_get_frame_no(void *)"
    extern "int sixel_frame_get_loop_no(void *)"
    extern "int sixel_frame_strip_alpha(void *, char *)"
    extern "int sixel_frame_resize(void *, int, int, int)"
    extern "int sixel_frame_clip(void *, int, int, int, int)"
    extern "int sixel_loader_new(void *, void *)"
    extern "void sixel_loader_ref(void *)"
    extern "void sixel_loader_unref(void *)"
    extern "int sixel_loader_setopt(void *, int, void *)"
    extern "int sixel_loader_load_file(void *, char *, void *)"
    extern "int sixel_helper_load_image_file(char *, int, int, int, char *, int, void *, int, int, void *, void *)"
    extern "int sixel_helper_write_image_file(char *, int, int, char *, int, char *, int, void *)"

    extern "int sixel_encoder_new(void *, void *)"
    extern "void * sixel_encoder_create(void)"
    extern "void sixel_encoder_ref(void *)"
    extern "void sixel_encoder_unref(void *)"
    extern "int sixel_encoder_set_cancel_flag(void *, int)"
    extern "int sixel_encoder_setopt(void *, int, char *)"
    extern "int sixel_encoder_encode(void *, char *)"
    extern "int sixel_encoder_encode_bytes(void *, char *, int, int, int, char *, int)"

    extern "int sixel_decoder_new(void *, void *)"
    extern "void * sixel_decoder_create(void)"
    extern "void sixel_decoder_ref(void *)"
    extern "void sixel_decoder_unref(void *)"
    extern "int sixel_decoder_setopt(void *, int, char *)"
    extern "int sixel_decoder_decode(void *)"

    # Helpers (scoped under Libsixel::API)
    module Util
      module_function

      def pack_ptr(addr)
        Fiddle::Pointer.new(addr)
      end

      def make_outptr
        Fiddle::Pointer.malloc(Fiddle::SIZEOF_VOIDP)
      end

      def read_outptr(p)
        size = Fiddle::SIZEOF_VOIDP
        raw = p[0, size]
        addr = raw.unpack1(size == 8 ? 'Q' : 'L')
        Fiddle::Pointer.new(addr)
      end

      def to_cstr(str)
        (str.nil? ? nil : str.to_s)
      end
    end

    module Err
      module_function
      def message(status)
        res = Libsixel::API.sixel_helper_format_error(status)
        return res if res.is_a?(String)
        begin
          Fiddle::Pointer.new(res).to_s
        rescue
          res.to_s
        end
      end
    end
  end # module API
end # module Libsixel
# emacs Local Variables:
# emacs mode: ruby
# emacs tab-width: 2
# emacs indent-tabs-mode: nil
# emacs ruby-indent-level: 2
# emacs End:
# vim: set expandtab ts=2 sts=2 sw=2 :
# EOF
