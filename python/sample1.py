#!/usr/bin/env python
#
# This code is in public domain
#

import ctypes
import ctypes.util

dllpath = ctypes.util.find_library("sixel")
sixel = ctypes.CDLL(dllpath)
sixel.sixel_easy_encode("-", None, None)
