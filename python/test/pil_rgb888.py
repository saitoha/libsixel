from PIL import Image
from libsixel import encoder
from libsixel import *

im = Image.open("../images/snake.png")
e = encoder.Encoder()
w, h = im.size
e.encode_bytes(im.tobytes(), w, h, SIXEL_PIXELFORMAT_RGB888, None)
