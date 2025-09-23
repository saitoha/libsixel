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
  class DecoderHandle
    attr_reader :ptr
    def initialize(ptr)
      @ptr = ptr
    end
  end
end

class Decoder
  def initialize(allocator: nil)
    out = Libsixel::API::Util.make_outptr
    status = Libsixel::API.sixel_decoder_new(out, allocator || 0)
    raise RuntimeError, Libsixel::API::Err.message(status) if Libsixel::API.failed?(status)
    @ptr = Libsixel::API::Util.read_outptr(out)
    ObjectSpace.define_finalizer(self, self.class.finalizer(@ptr))
  end

  def self.finalizer(ptr)
    proc { Libsixel::API.sixel_decoder_unref(ptr) if ptr && ptr.to_i != 0 }
  end

  def setopt(flag, opt)
    int_flag = flag.is_a?(Integer) ? flag : flag.to_s.getbyte(0)
    cstr = Libsixel::API::Util.to_cstr(opt)
    status = Libsixel::API.sixel_decoder_setopt(@ptr, int_flag, cstr)
    raise RuntimeError, Libsixel::API::Err.message(status) if Libsixel::API.failed?(status)
    nil
  end

  def decode
    status = Libsixel::API.sixel_decoder_decode(@ptr)
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
