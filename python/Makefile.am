
CLEANFILES=libsixel/*.pyc

clean-local:
	-rm -rf libsixel_python.egg-info build dist

if HAVE_PYTHON
libsixeldir = $(pythondir)/libsixel
libsixel_PYTHON = \
    libsixel/__init__.py \
    libsixel/encoder.py \
    libsixel/decoder.py
endif

EXTRA_DIST = \
    setup.py

check: check-am

build-testdata:
	python -c "import PIL"  # confirm PIL is installed
	python -c "from PIL import Image; print(Image.open('../images/snake.png').tobytes())" > ../images/snake.dump-rgb888
	python -c "from PIL import Image; print(Image.open('../images/snake-palette.png').tobytes())" > ../images/snake.dump-pal8-body
	python -c "from PIL import Image; print(''.join(map(chr, Image.open('../images/snake-palette.png').getpalette())))" > ../images/snake.dump-pal8-pal
