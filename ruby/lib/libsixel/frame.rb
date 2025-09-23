class Frame
  def initialize(allocator: nil)
    outpp = Libsixel::API::Util.make_outptr
    status = Libsixel::API.sixel_frame_new(outpp, allocator || 0)
    raise RuntimeError, Libsixel::API::Err.message(status) if Libsixel::API.failed?(status)
    @ptr = Libsixel::API::Util.read_outptr(outpp)
    ObjectSpace.define_finalizer(self, self.class.finalizer(@ptr))
  end

  def self.finalizer(ptr)
    proc { Libsixel::API.sixel_frame_unref(ptr) if ptr && ptr.to_i != 0 }
  end

  def ptr; @ptr; end

  def ref; Libsixel::API.sixel_frame_ref(@ptr); nil; end
  def unref; Libsixel::API.sixel_frame_unref(@ptr); nil; end

  def init(pixels:, width:, height:, pixelformat:, palette: nil, ncolors: -1)
    px = pixels.is_a?(String) ? pixels : pixels.pack('C*')
    pal = palette && (palette.is_a?(String) ? palette : palette.pack('C*'))
    status = Libsixel::API.sixel_frame_init(@ptr, px, width.to_i, height.to_i, pixelformat.to_i, pal, ncolors.to_i)
    raise RuntimeError, Libsixel::API::Err.message(status) if Libsixel::API.failed?(status)
    nil
  end

  def pixels
    Fiddle::Pointer.new(Libsixel::API.sixel_frame_get_pixels(@ptr)).to_s
  end

  def palette
    Fiddle::Pointer.new(Libsixel::API.sixel_frame_get_palette(@ptr)).to_s
  end

  def width; Libsixel::API.sixel_frame_get_width(@ptr); end
  def height; Libsixel::API.sixel_frame_get_height(@ptr); end
  def ncolors; Libsixel::API.sixel_frame_get_ncolors(@ptr); end
  def pixelformat; Libsixel::API.sixel_frame_get_pixelformat(@ptr); end
  def transparent; Libsixel::API.sixel_frame_get_transparent(@ptr); end
  def multiframe; Libsixel::API.sixel_frame_get_multiframe(@ptr); end
  def delay; Libsixel::API.sixel_frame_get_delay(@ptr); end
  def frame_no; Libsixel::API.sixel_frame_get_frame_no(@ptr); end
  def loop_no; Libsixel::API.sixel_frame_get_loop_no(@ptr); end

  def strip_alpha(bgcolor_rgb)
    # bgcolor_rgb: 3-byte string or array [r,g,b]
    buf = bgcolor_rgb.is_a?(String) ? bgcolor_rgb : bgcolor_rgb.pack('C*')
    Libsixel::API.sixel_frame_strip_alpha(@ptr, buf)
  end

  def resize(width:, height:, resample: 0)
    status = Libsixel::API.sixel_frame_resize(@ptr, width.to_i, height.to_i, resample.to_i)
    raise RuntimeError, Libsixel::API::Err.message(status) if Libsixel::API.failed?(status)
    nil
  end

  def clip(x:, y:, width:, height:)
    status = Libsixel::API.sixel_frame_clip(@ptr, x.to_i, y.to_i, width.to_i, height.to_i)
    raise RuntimeError, Libsixel::API::Err.message(status) if Libsixel::API.failed?(status)
    nil
  end
end

