# -*- coding: utf-8 -*-

from setuptools import setup, find_packages
__version__ = '0.4.0'
__license__ = 'MIT'
__author__ = 'Hayaki Saito'

import inspect
import os

filename = inspect.getfile(inspect.currentframe())
dirpath = os.path.abspath(os.path.dirname(filename))
long_description = open(os.path.join(dirpath, "README.rst")).read()

setup(name                  = 'libsixel-python',
      version               = __version__,
      description           = 'libsixel binding for Python',
      long_description      = long_description,
      py_modules            = ['libsixel'],
      classifiers           = ['Development Status :: 4 - Beta',
                               'Topic :: Terminals',
                               'Environment :: Console',
                               'Intended Audience :: End Users/Desktop',
                               'License :: OSI Approved :: MIT License',
                               'Programming Language :: Python'
                               ],
      keywords              = 'sixel libsixel terminal codec',
      author                = __author__,
      author_email          = 'saitoha@me.com',
      url                   = 'https://github.com/saitoha/libsixel',
      license               = __license__,
      packages              = find_packages(exclude=[]),
      zip_safe              = False,
      include_package_data  = False,
      install_requires      = []
      )
