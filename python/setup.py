# -*- coding: utf-8 -*-

from setuptools import setup, find_packages
__version__ = '0.5.1'
__license__ = 'MIT'
__author__ = 'Hayaki Saito'

import inspect
import os

filename = inspect.getfile(inspect.currentframe())
dirpath = os.path.abspath(os.path.dirname(filename))
long_description = open(os.path.join(dirpath, "README.rst")).read()

# Wheel mode builds a separate distribution name that bundles libsixel.
# This keeps the traditional "libsixel-python" package unchanged while
# allowing a self-contained wheel named "libsixel-wheel".
wheel_mode = os.environ.get("LIBSIXEL_WHEEL") == "1"
package_name = "libsixel-wheel" if wheel_mode else "libsixel-python"
package_data = {"libsixel": ["_libs/*"]} if wheel_mode else {}

setup(name                  = package_name,
      version               = __version__,
      description           = 'libsixel binding for Python',
      long_description      = long_description,
      classifiers           = ['Development Status :: 4 - Production/Stable',
                               'Topic :: Terminals',
                               'Environment :: Console',
                               'Intended Audience :: End Users/Desktop',
                               'Programming Language :: Python'
                               ],
      keywords              = 'sixel libsixel terminal codec',
      author                = __author__,
      author_email          = 'saitoha@me.com',
      url                   = 'https://github.com/saitoha/libsixel',
      license               = __license__,
      packages              = find_packages(exclude=[]),
      package_data          = package_data,
      zip_safe              = False,
      include_package_data  = wheel_mode,
      install_requires      = []
      )
