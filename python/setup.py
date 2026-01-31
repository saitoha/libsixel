# -*- coding: utf-8 -*-

from setuptools import setup, find_packages, Extension
__version__ = '0.5.1'
__license__ = 'MIT'
__author__ = 'Hayaki Saito'

import inspect
import os
import sys

filename = inspect.getfile(inspect.currentframe())
dirpath = os.path.abspath(os.path.dirname(filename))
long_description = open(os.path.join(dirpath, "README.rst")).read()

bundle_libdir = os.environ.get("LIBSIXEL_LIBDIR")
bundle_mode = bundle_libdir is not None
package_name = "libsixel_wheel"
package_data = {"libsixel": ["_libs/*"]} if bundle_mode else {}
wheel_ext_modules = []

if bundle_mode:
    libdir = bundle_libdir

    include_dir = os.path.abspath(os.path.join(dirpath, "..", "include"))
    runtime_dirs = []
    extra_link_args = []

    if sys.platform == "darwin":
        extra_link_args = ["-Wl,-rpath,@loader_path/_libs"]
    elif os.name == "posix":
        runtime_dirs = ["$ORIGIN/_libs"]

    wheel_ext_modules = [
        Extension(
            "libsixel._wheel_ext",
            sources=[os.path.join("libsixel", "_wheel_ext.c")],
            include_dirs=[include_dir],
            libraries=["sixel"],
            library_dirs=[libdir],
            runtime_library_dirs=runtime_dirs,
            extra_link_args=extra_link_args,
        )
    ]

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
      ext_modules           = wheel_ext_modules,
      zip_safe              = False,
      include_package_data  = bundle_mode,
      install_requires      = []
      )
