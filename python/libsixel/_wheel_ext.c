/*
 * Copyright (c) 2014-2025 Hayaki Saito
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * Minimal C extension for libsixel-python wheels.
 *
 * This module exists to ensure the wheel is tagged as a platform build. It
 * exposes the linked libsixel version to confirm the shared library link.
 */

#include <Python.h>

#include "sixel.h"

static PyObject *
libsixel_ext_linked_version(PyObject *self, PyObject *args)
{
    const char *version;

    (void)self;
    (void)args;

    version = LIBSIXEL_VERSION;
    if (version == NULL) {
        Py_RETURN_NONE;
    }
    return PyUnicode_FromString(version);
}

static PyMethodDef libsixel_ext_methods[] = {
    {"linked_version", libsixel_ext_linked_version, METH_NOARGS,
     "Return the linked libsixel version string."},
    {NULL, NULL, 0, NULL},
};

static struct PyModuleDef libsixel_ext_module = {
    PyModuleDef_HEAD_INIT,
    "_wheel_ext",
    "libsixel helper extension.",
    -1,
    libsixel_ext_methods,
    NULL,
    NULL,
    NULL,
    NULL
};

PyMODINIT_FUNC
PyInit__wheel_ext(void)
{
    return PyModule_Create(&libsixel_ext_module);
}
