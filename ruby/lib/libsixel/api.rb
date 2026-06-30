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

    # Bundled library loading flow (used by platform gems):
    #
    #   +------------------------------+
    #   | lib/libsixel/_libs contains? |
    #   +------------------------------+
    #              | yes
    #              v
    #   try exact file path with dlload
    #              |
    #              v
    #         loaded? ------ no ------+
    #            | yes                |
    #            v                    |
    #          done                   |
    #                                 v
    #                        fallback to system names
    #
    # This keeps backward compatibility: classic gems still work because the
    # system probing path is preserved after bundled-path probing.
    BUNDLED_LIB_DIR = File.expand_path('_libs', __dir__)

    # Search and load libsixel shared library
    LIB_CANDIDATES = [
      "sixel", "libsixel", "sixel-1", "libsixel-1", "msys-sixel", "cygsixel",
      # macOS explicit filename fallback
      "libsixel.dylib"
    ]

    bundled_candidates = []
    if Dir.exist?(BUNDLED_LIB_DIR)
      bundled_candidates = Dir.glob(File.join(BUNDLED_LIB_DIR, '*'))
    end

    loaded = false
    (bundled_candidates + LIB_CANDIDATES).each do |name|
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
    SIXEL_COM_ERROR = (SIXEL_FALSE | 0x0c00) unless const_defined?(:SIXEL_COM_ERROR)
    SIXEL_WIC_ERROR = (SIXEL_FALSE | 0x0d00) unless const_defined?(:SIXEL_WIC_ERROR)
    # DRCS option layout quick map:
    #   MMV : CHARSET : PATH
    #     |      |        +-> optional tile sink ("-" keeps stdout,
    #     |      |            blank discards tiles)
    #     |      +-> charset number (range depends on MMV,
    #     |            defaults to 1)
    #     +-> mapping revision (0..2, defaults to 2)
    SIXEL_OPTFLAG_DRCS = '@' unless const_defined?(:SIXEL_OPTFLAG_DRCS)
    SIXEL_OPTFLAG_ORMODE = 'O' unless const_defined?(:SIXEL_OPTFLAG_ORMODE)
    SIXEL_OPTFLAG_INPUT = 'i' unless const_defined?(:SIXEL_OPTFLAG_INPUT)
    SIXEL_OPTFLAG_OUTPUT = 'o' unless const_defined?(:SIXEL_OPTFLAG_OUTPUT)
    SIXEL_OPTFLAG_OUTFILE = 'o' unless const_defined?(:SIXEL_OPTFLAG_OUTFILE)
    SIXEL_OPTFLAG_7BIT_MODE = '7' unless const_defined?(:SIXEL_OPTFLAG_7BIT_MODE)
    SIXEL_OPTFLAG_8BIT_MODE = '8' unless const_defined?(:SIXEL_OPTFLAG_8BIT_MODE)
    SIXEL_OPTFLAG_COLORS = 'p' unless const_defined?(:SIXEL_OPTFLAG_COLORS)
    SIXEL_OPTFLAG_MONOCHROME = 'e' unless const_defined?(:SIXEL_OPTFLAG_MONOCHROME)
    SIXEL_OPTFLAG_INSECURE = 'k' unless const_defined?(:SIXEL_OPTFLAG_INSECURE)
    SIXEL_OPTFLAG_INVERT = 'i' unless const_defined?(:SIXEL_OPTFLAG_INVERT)
    SIXEL_OPTFLAG_HIGH_COLOR = 'I' unless const_defined?(:SIXEL_OPTFLAG_HIGH_COLOR)
    SIXEL_OPTFLAG_USE_MACRO = 'u' unless const_defined?(:SIXEL_OPTFLAG_USE_MACRO)
    SIXEL_OPTFLAG_MACRO_NUMBER = 'n' unless const_defined?(:SIXEL_OPTFLAG_MACRO_NUMBER)
    SIXEL_OPTFLAG_COMPLEXION_SCORE = 'C' unless const_defined?(:SIXEL_OPTFLAG_COMPLEXION_SCORE)
    SIXEL_OPTFLAG_IGNORE_DELAY = 'g' unless const_defined?(:SIXEL_OPTFLAG_IGNORE_DELAY)
    SIXEL_OPTFLAG_STATIC = 'S' unless const_defined?(:SIXEL_OPTFLAG_STATIC)
    SIXEL_OPTFLAG_SIMILARITY = 'S' unless const_defined?(:SIXEL_OPTFLAG_SIMILARITY)
    SIXEL_OPTFLAG_SIZE = 's' unless const_defined?(:SIXEL_OPTFLAG_SIZE)
    SIXEL_OPTFLAG_EDGE = 'e' unless const_defined?(:SIXEL_OPTFLAG_EDGE)
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
      # The LUT flag mirrors img2sixel's -~ switch.  ASCII flow:
    #
    #       auto
    #        |
    #   +----+----+---------+
    #   |         |
    # classic  none        certified
    # (5/6bit) (no cache)  (certlut)
    #
    SIXEL_OPTFLAG_HAS_GRI_ARG_LIMIT = 'R' unless const_defined?(:SIXEL_OPTFLAG_HAS_GRI_ARG_LIMIT)
    SIXEL_OPTFLAG_PRECISION = '.' unless const_defined?(:SIXEL_OPTFLAG_PRECISION)
    # Thread override mirrors img2sixel's -= switch for bindings.
    SIXEL_OPTFLAG_THREADS = '=' unless const_defined?(:SIXEL_OPTFLAG_THREADS)
    SIXEL_OPTFLAG_6REVERSIBLE = '6' unless const_defined?(:SIXEL_OPTFLAG_6REVERSIBLE)
    SIXEL_OPTFLAG_LUT_POLICY = '~' unless const_defined?(:SIXEL_OPTFLAG_LUT_POLICY)
    SIXEL_OPTFLAG_FIND_LARGEST = 'f' unless const_defined?(:SIXEL_OPTFLAG_FIND_LARGEST)
    SIXEL_OPTFLAG_SELECT_COLOR = 's' unless const_defined?(:SIXEL_OPTFLAG_SELECT_COLOR)
    SIXEL_OPTFLAG_CLUSTERING_COLORSPACE = 'X' unless const_defined?(:SIXEL_OPTFLAG_CLUSTERING_COLORSPACE)
    SIXEL_OPTFLAG_WORKING_COLORSPACE = 'W' unless const_defined?(:SIXEL_OPTFLAG_WORKING_COLORSPACE)
    SIXEL_OPTFLAG_OUTPUT_COLORSPACE = 'U' unless const_defined?(:SIXEL_OPTFLAG_OUTPUT_COLORSPACE)
    SIXEL_OPTFLAG_TRANSPARENT_POLICY = 'A' unless const_defined?(:SIXEL_OPTFLAG_TRANSPARENT_POLICY)
    SIXEL_OPTFLAG_TRANSPARENT_OFFSET = '+' unless const_defined?(:SIXEL_OPTFLAG_TRANSPARENT_OFFSET)
    SIXEL_OPTFLAG_6DELTA_THRESHOLD = 'Z' unless const_defined?(:SIXEL_OPTFLAG_6DELTA_THRESHOLD)
    SIXEL_OPTFLAG_6DELTA_ERROR = 'Y' unless const_defined?(:SIXEL_OPTFLAG_6DELTA_ERROR)
    SIXEL_OPTFLAG_LOADERS = 'L' unless const_defined?(:SIXEL_OPTFLAG_LOADERS)
    SIXEL_OPTFLAG_MAPFILE_OUTPUT = 'M' unless const_defined?(:SIXEL_OPTFLAG_MAPFILE_OUTPUT)
    SIXEL_OPTFLAG_QUANTIZE_MODEL = 'Q' unless const_defined?(:SIXEL_OPTFLAG_QUANTIZE_MODEL)
    SIXEL_OPTFLAG_MAPFILE = 'm' unless const_defined?(:SIXEL_OPTFLAG_MAPFILE)
    SIXEL_OPTFLAG_CROP = 'c' unless const_defined?(:SIXEL_OPTFLAG_CROP)
    SIXEL_OPTFLAG_WIDTH = 'w' unless const_defined?(:SIXEL_OPTFLAG_WIDTH)
    SIXEL_OPTFLAG_HEIGHT = 'h' unless const_defined?(:SIXEL_OPTFLAG_HEIGHT)
    SIXEL_OPTFLAG_RESAMPLING = 'r' unless const_defined?(:SIXEL_OPTFLAG_RESAMPLING)
    SIXEL_OPTFLAG_QUALITY = 'q' unless const_defined?(:SIXEL_OPTFLAG_QUALITY)
    SIXEL_OPTFLAG_LOOPMODE = 'l' unless const_defined?(:SIXEL_OPTFLAG_LOOPMODE)
    SIXEL_OPTFLAG_START_FRAME = 'T' unless const_defined?(:SIXEL_OPTFLAG_START_FRAME)
    SIXEL_OPTFLAG_PALETTE_TYPE = 't' unless const_defined?(:SIXEL_OPTFLAG_PALETTE_TYPE)
    SIXEL_OPTFLAG_BUILTIN_PALETTE = 'b' unless const_defined?(:SIXEL_OPTFLAG_BUILTIN_PALETTE)
    SIXEL_OPTFLAG_ENCODE_POLICY = 'E' unless const_defined?(:SIXEL_OPTFLAG_ENCODE_POLICY)
    SIXEL_OPTFLAG_BGCOLOR = 'B' unless const_defined?(:SIXEL_OPTFLAG_BGCOLOR)
    SIXEL_OPTFLAG_PENETRATE = 'P' unless const_defined?(:SIXEL_OPTFLAG_PENETRATE)
    SIXEL_OPTFLAG_PIPE_MODE = 'D' unless const_defined?(:SIXEL_OPTFLAG_PIPE_MODE)
    SIXEL_OPTFLAG_VERBOSE = 'v' unless const_defined?(:SIXEL_OPTFLAG_VERBOSE)
    SIXEL_OPTFLAG_VERSION = 'V' unless const_defined?(:SIXEL_OPTFLAG_VERSION)
    SIXEL_OPTFLAG_HELP = 'H' unless const_defined?(:SIXEL_OPTFLAG_HELP)
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

    singleton_class.class_eval do
      alias_method :__raw_sixel_loader_setopt, :sixel_loader_setopt
      alias_method :__raw_sixel_loader_load_file, :sixel_loader_load_file
    end

    def self.sixel_loader_load_file(loader, filename, callback)
      path =
        if filename.respond_to?(:to_path)
          filename.to_path
        elsif filename.is_a?(String)
          filename
        else
          raise TypeError, 'filename must be a String or respond to #to_path'
        end
      unless callback.nil? || callback.is_a?(Integer) || callback.is_a?(Fiddle::Pointer)
        raise TypeError, 'callback must be nil, Integer, or Fiddle::Pointer'
      end

      __raw_sixel_loader_load_file(loader, path, callback)
    end

    def self.__loader_setopt_option_bucket(option)
      case option
      when 1, 2, 3, 5, 6, 10, 11
        :int
      when 4
        :bgcolor
      when 8
        :loader_order
      when 7, 9
        :pointer
      else
        :legacy
      end
    end

    def self.__loader_setopt_legacy_coerce(value)
      if value.nil? || value.is_a?(String) || value.is_a?(Fiddle::Pointer)
        value
      elsif value.respond_to?(:to_path)
        value.to_path
      elsif value.respond_to?(:to_str)
        value.to_str
      else
        raise TypeError, 'value must be nil, String-like, Path-like, or Fiddle::Pointer'
      end
    end

    def self.__loader_setopt_int_pointer(value)
      parsed = 0
      pointer = nil
      packed = nil

      return nil if value.nil?

      begin
        parsed = Integer(value)
      rescue TypeError, ArgumentError
        raise TypeError, 'integer option value must be Integer-like or nil'
      end

      begin
        packed = [parsed].pack('i!')
      rescue RangeError
        raise ArgumentError, 'integer option value is out of range for C int'
      end

      pointer = Fiddle::Pointer.malloc(Fiddle::SIZEOF_INT)
      pointer[0, Fiddle::SIZEOF_INT] = packed
      pointer
    end

    def self.__loader_setopt_bgcolor_pointer(value)
      components = nil
      packed = nil
      pointer = nil

      return nil if value.nil?

      if !value.respond_to?(:to_ary)
        raise TypeError, 'bgcolor must be [r, g, b] or nil'
      end
      components = value.to_ary
      if !components.is_a?(Array) || components.length != 3
        raise ArgumentError, 'bgcolor must contain exactly 3 components'
      end

      components = components.map do |component|
        parsed = 0
        begin
          parsed = Integer(component)
        rescue TypeError, ArgumentError
          raise TypeError, 'bgcolor components must be integers in 0..255'
        end
        if parsed < 0 || parsed > 255
          raise ArgumentError, 'bgcolor components must be in 0..255'
        end
        parsed
      end

      packed = components.pack('C3')
      pointer = Fiddle::Pointer.malloc(3)
      pointer[0, 3] = packed
      pointer
    end

    def self.__loader_setopt_loader_order(value)
      if value.nil? || value.is_a?(String)
        value
      elsif value.respond_to?(:to_path)
        value.to_path
      elsif value.respond_to?(:to_str)
        value.to_str
      else
        raise TypeError, 'loader_order must be nil, String-like, or Path-like'
      end
    end

    def self.__loader_setopt_pointer(value, label)
      return nil if value.nil?
      return value if value.is_a?(Fiddle::Pointer)
      return Fiddle::Pointer.new(value) if value.is_a?(Integer)
      raise TypeError, "#{label} must be nil, Integer, or Fiddle::Pointer"
    end

    def self.sixel_loader_setopt(loader, option, value)
      raise TypeError, 'option must be an Integer' unless option.is_a?(Integer)

      keepalive = nil
      coerced = nil
      bucket = __loader_setopt_option_bucket(option)

      case bucket
      when :int
        keepalive = __loader_setopt_int_pointer(value)
        coerced = keepalive
      when :bgcolor
        keepalive = __loader_setopt_bgcolor_pointer(value)
        coerced = keepalive
      when :loader_order
        coerced = __loader_setopt_loader_order(value)
      when :pointer
        if option == 7
          keepalive = __loader_setopt_pointer(value, 'cancel_flag')
        else
          keepalive = __loader_setopt_pointer(value, 'context')
        end
        coerced = keepalive
      else
        coerced = __loader_setopt_legacy_coerce(value)
      end

      __raw_sixel_loader_setopt(loader, option, coerced)
    end
    extern "int sixel_helper_load_image_file(char *, int, int, int, char *, int, void *, int, int, void *, void *)"
    extern "int sixel_helper_write_image_file(char *, int, int, char *, int, char *, int, void *)"

    extern "int sixel_encoder_new(void *, void *)"
    extern "void * sixel_encoder_create(void)"
    extern "void sixel_encoder_ref(void *)"
    extern "void sixel_encoder_unref(void *)"
    extern "int sixel_encoder_set_cancel_flag(void *, int)"
    extern "int sixel_encoder_set_accumulation_buffer(void *, char *, int, int, int)"
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
