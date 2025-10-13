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
class Dither
  def initialize(ncolors:, allocator: nil)
    outpp = Libsixel::API::Util.make_outptr
    status = Libsixel::API.sixel_dither_new(outpp, ncolors.to_i, allocator || 0)
    raise RuntimeError, Libsixel::API::Err.message(status) if Libsixel::API.failed?(status)
    @ptr = Libsixel::API::Util.read_outptr(outpp)
    ObjectSpace.define_finalizer(self, self.class.finalizer(@ptr))
  end

  def self.get(builtin_id)
    ptr = Libsixel::API.sixel_dither_get(builtin_id.to_i)
    inst = allocate
    inst.instance_variable_set(:@ptr, ptr)
    inst
  end

  def self.finalizer(ptr)
    proc { Libsixel::API.sixel_dither_unref(ptr) if ptr && ptr.to_i != 0 }
  end

  def ptr; @ptr; end

  def ref; Libsixel::API.sixel_dither_ref(@ptr); nil; end
  def unref; Libsixel::API.sixel_dither_unref(@ptr); nil; end

  def initialize_palette(data, width:, height:, pixelformat:, method_for_largest:, method_for_rep:, quality_mode:)
    buf = data.is_a?(String) ? data : data.pack('C*')
    status = Libsixel::API.sixel_dither_initialize(@ptr, buf, width.to_i, height.to_i,
                                                   pixelformat.to_i, method_for_largest.to_i,
                                                   method_for_rep.to_i, quality_mode.to_i)
    raise RuntimeError, Libsixel::API::Err.message(status) if Libsixel::API.failed?(status)
    nil
  end

  def set_diffusion_type(m)
    Libsixel::API.sixel_dither_set_diffusion_type(@ptr, m.to_i); nil
  end

  def set_diffusion_scan(m)
    Libsixel::API.sixel_dither_set_diffusion_scan(@ptr, m.to_i); nil
  end

  def num_palette_colors
    Libsixel::API.sixel_dither_get_num_of_palette_colors(@ptr)
  end

  def num_histogram_colors
    Libsixel::API.sixel_dither_get_num_of_histogram_colors(@ptr)
  end

  def palette
    p = Libsixel::API.sixel_dither_get_palette(@ptr)
    Fiddle::Pointer.new(p).to_s
  end

  def set_palette(raw)
    buf = raw.is_a?(String) ? raw : raw.pack('C*')
    Libsixel::API.sixel_dither_set_palette(@ptr, buf); nil
  end

  def set_complexion_score(score)
    Libsixel::API.sixel_dither_set_complexion_score(@ptr, score.to_i); nil
  end

  def set_body_only(v)
    Libsixel::API.sixel_dither_set_body_only(@ptr, v.to_i); nil
  end

  def set_optimize_palette(v)
    Libsixel::API.sixel_dither_set_optimize_palette(@ptr, v.to_i); nil
  end

  def set_pixelformat(pf)
    Libsixel::API.sixel_dither_set_pixelformat(@ptr, pf.to_i); nil
  end

  def set_transparent(idx)
    Libsixel::API.sixel_dither_set_transparent(@ptr, idx.to_i); nil
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
