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
module Libsixel
  class EncoderHandle
    attr_reader :ptr
    def initialize(ptr)
      @ptr = ptr
    end
  end
end

# Top-level Encoder for backward compatibility
class Encoder
  def initialize(allocator: nil)
    @allocator = allocator
    out = Libsixel::API::Util.make_outptr
    status = Libsixel::API.sixel_encoder_new(out, allocator || 0)
    raise RuntimeError, Libsixel::API::Err.message(status) if Libsixel::API.failed?(status)
    @ptr = Libsixel::API::Util.read_outptr(out)
    ObjectSpace.define_finalizer(self, self.class.finalizer(@ptr))
  end

  def self.finalizer(ptr)
    proc { Libsixel::API.sixel_encoder_unref(ptr) if ptr && ptr.to_i != 0 }
  end

  def ptr
    @ptr
  end

  def ref
    Libsixel::API.sixel_encoder_ref(@ptr) if @ptr && @ptr.to_i != 0
    nil
  end

  def unref
    return nil unless @ptr && @ptr.to_i != 0

    ptr = @ptr
    @ptr = nil
    ObjectSpace.undefine_finalizer(self)
    Libsixel::API.sixel_encoder_unref(ptr)
    nil
  end

  alias close unref

  # setopt: flag は 1 文字。opt は文字列 or 数値
  # Width/height accept auto, numeric, percent, pixel (<number>px), and
  # terminal cell (<number>c) units to stay aligned with the CLI parser.
  def setopt(flag, opt)
    int_flag = flag.is_a?(Integer) ? flag : flag.to_s.getbyte(0)
    cstr = Libsixel::API::Util.to_cstr(opt)
    status = Libsixel::API.sixel_encoder_setopt(@ptr, int_flag, cstr)
    raise RuntimeError, Libsixel::API::Err.message(status) if Libsixel::API.failed?(status)
    nil
  end

  def encode(filename)
    status = Libsixel::API.sixel_encoder_encode(@ptr, filename.to_s)
    raise RuntimeError, Libsixel::API::Err.message(status) if Libsixel::API.failed?(status)
    nil
  end

  # Optional direct encoding API for already-decoded pixel buffers.
  #
  # Validation flow:
  #   1. Normalize geometry and pixel format.
  #   2. Resolve depth bytes/pixel via libsixel helper.
  #   3. Verify the Ruby buffer has enough bytes before crossing FFI.
  #
  # This keeps short-input failures deterministic at Ruby level instead of
  # leaving behavior to downstream C calls.
  def encode_bytes(bytes, width:, height:, pixelformat:, palette: nil)
    width_i = width.to_i
    height_i = height.to_i
    pixelformat_i = pixelformat.to_i
    pixel_depth = Libsixel::API.sixel_helper_compute_depth(pixelformat_i)

    raise ArgumentError, 'width must be positive' unless width_i.positive?
    raise ArgumentError, 'height must be positive' unless height_i.positive?
    unless pixel_depth.is_a?(Integer) && pixel_depth.positive?
      raise ArgumentError, "invalid pixelformat: #{pixelformat_i}"
    end

    buffer =
      if bytes.is_a?(String)
        bytes
      elsif bytes.respond_to?(:to_str)
        bytes.to_str
      else
        raise TypeError, 'bytes must be a String or String-like object'
      end
    required_bytes = width_i * height_i * pixel_depth
    if buffer.bytesize < required_bytes
      raise ArgumentError,
            "pixel buffer is too short: got #{buffer.bytesize}, need #{required_bytes}"
    end

    palette_ptr = palette ? palette.pack('C*') : nil
    status = Libsixel::API.sixel_encoder_encode_bytes(
      @ptr, buffer, width_i, height_i, pixelformat_i,
      palette_ptr, palette ? palette.length : 0
    )
    raise RuntimeError, Libsixel::API::Err.message(status) if Libsixel::API.failed?(status)
    nil
  end
end

# emacs Local Variables:
# emacs mode: ruby
# emacs tab-width: 2
# emacs indent-tabs-mode: nil
# emacs ruby-indent-level: 2
# emacs End:
# vim: set expandtab ts=2 sts=2 sw=2 :
# EOF
