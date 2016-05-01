from PIL import Image
from libsixel import encoder
from libsixel import *

im = Image.open("../images/snake-palette.png")
e = encoder.Encoder()
w, h = im.size
pal = ''.join(map(chr, im.getpalette()))
e.encode_bytes(im.tobytes(), w, h, SIXEL_PIXELFORMAT_PAL8, im.getpalette())
