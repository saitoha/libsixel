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
class Output
  def initialize(write_proc:, priv: nil, allocator: nil)
    @write_proc = write_proc
    @priv = priv
    @allocator = allocator || 0

    @cb = Fiddle::Closure::BlockCaller.new(
      Fiddle::TYPE_INT,
      [Fiddle::TYPE_VOIDP, Fiddle::TYPE_INT, Fiddle::TYPE_VOIDP]
    ) do |data, size, _priv|
      chunk = Fiddle::Pointer.new(data)[0, size]
      @write_proc.call(chunk, @priv)
      size
    end

    outpp = Libsixel::API::Util.make_outptr
    status = Libsixel::API.sixel_output_new(outpp, @cb, 0, @allocator)
    raise RuntimeError, Libsixel::API::Err.message(status) if Libsixel::API.failed?(status)
    @ptr = Libsixel::API::Util.read_outptr(outpp)
    ObjectSpace.define_finalizer(self, self.class.finalizer(@ptr))
  end

  def self.finalizer(ptr)
    proc { Libsixel::API.sixel_output_unref(ptr) if ptr && ptr.to_i != 0 }
  end

  def ptr; @ptr; end

  def ref; Libsixel::API.sixel_output_ref(@ptr); end
  def unref; Libsixel::API.sixel_output_unref(@ptr); end

  def get_8bit_availability
    Libsixel::API.sixel_output_get_8bit_availability(@ptr)
  end

  def set_8bit_availability(v)
    Libsixel::API.sixel_output_set_8bit_availability(@ptr, v.to_i)
    nil
  end

  def set_gri_arg_limit(v)
    Libsixel::API.sixel_output_set_gri_arg_limit(@ptr, v.to_i)
    nil
  end

  def set_penetrate_multiplexer(v)
    Libsixel::API.sixel_output_set_penetrate_multiplexer(@ptr, v.to_i)
    nil
  end

  def set_skip_dcs_envelope(v)
    Libsixel::API.sixel_output_set_skip_dcs_envelope(@ptr, v.to_i)
    nil
  end

  def set_skip_header(v)
    Libsixel::API.sixel_output_set_skip_header(@ptr, v.to_i)
    nil
  end

  def set_palette_type(t)
    Libsixel::API.sixel_output_set_palette_type(@ptr, t.to_i)
    nil
  end

  def set_ormode(v)
    Libsixel::API.sixel_output_set_ormode(@ptr, v.to_i)
    nil
  end

  def set_encode_policy(p)
    Libsixel::API.sixel_output_set_encode_policy(@ptr, p.to_i)
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
