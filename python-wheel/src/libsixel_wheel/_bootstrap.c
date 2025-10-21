#define PY_SSIZE_T_CLEAN
#include <Python.h>

#ifdef _WIN32
#include <windows.h>
#include <wchar.h>
#else
#include <dlfcn.h>
#endif

#include <stddef.h>

static PyObject *
bootstrap_load(PyObject *self, PyObject *args, PyObject *kwargs)
{
    PyObject *path_obj;
    PyObject *unicode_path;
    PyObject *bytes_path;
    PyObject *result;
    static char *kwlist[] = {"path", NULL};
    int parse_ok;
#ifdef _WIN32
    wchar_t *wide_path;
    wchar_t *dir_buffer;
    wchar_t *separator;
    Py_ssize_t wide_length;
    Py_ssize_t index;
    size_t dir_length;
    DLL_DIRECTORY_COOKIE cookie;
    HMODULE module;
#else
    const char *c_path;
    void *handle;
#endif

    path_obj = NULL;
    unicode_path = NULL;
    bytes_path = NULL;
    result = NULL;
#ifdef _WIN32
    wide_path = NULL;
    dir_buffer = NULL;
    separator = NULL;
    wide_length = 0;
    index = 0;
    dir_length = 0U;
    cookie = NULL;
    module = NULL;
#else
    c_path = NULL;
    handle = NULL;
#endif

    parse_ok = PyArg_ParseTupleAndKeywords(
        args,
        kwargs,
        "O",
        kwlist,
        &path_obj
    );
    if (parse_ok == 0) {
        return NULL;
    }
    if (PyUnicode_FSDecoder(path_obj, &unicode_path) == 0) {
        return NULL;
    }
#ifdef _WIN32
    wide_path = PyUnicode_AsWideCharString(unicode_path, &wide_length);
    if (wide_path == NULL) {
        goto exit;
    }
    if (wide_length == 0) {
        PyErr_SetString(PyExc_ValueError, "path must not be empty");
        goto exit;
    }
    index = wide_length - 1;
    while (index >= 0) {
        if (wide_path[index] == L'\\' || wide_path[index] == L'/') {
            separator = wide_path + index;
            break;
        }
        index -= 1;
    }
    if (separator != NULL) {
        dir_length = (size_t)(separator - wide_path);
        dir_buffer = PyMem_RawMalloc((dir_length + 1U) * sizeof(wchar_t));
        if (dir_buffer == NULL) {
            PyErr_NoMemory();
            goto exit;
        }
        wcsncpy(dir_buffer, wide_path, dir_length);
        dir_buffer[dir_length] = L'\0';
        cookie = AddDllDirectory(dir_buffer);
        if (cookie == NULL) {
            PyErr_SetFromWindowsErr(0);
            goto exit;
        }
    }
    module = LoadLibraryExW(
        wide_path,
        NULL,
        LOAD_LIBRARY_SEARCH_DEFAULT_DIRS |
            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR
    );
    if (module == NULL) {
        PyErr_SetFromWindowsErr(0);
        goto exit;
    }
#else
    if (PyUnicode_FSConverter(unicode_path, &bytes_path) == 0) {
        goto exit;
    }
    c_path = PyBytes_AsString(bytes_path);
    if (c_path == NULL) {
        goto exit;
    }
    handle = dlopen(
        c_path,
        RTLD_NOW
#ifdef RTLD_GLOBAL
            | RTLD_GLOBAL
#endif
    );
    if (handle == NULL) {
        const char *error_text;
        error_text = dlerror();
        if (error_text == NULL) {
            error_text = "dlopen failed";
        }
        PyErr_SetString(PyExc_OSError, error_text);
        goto exit;
    }
#endif
    result = PyBool_FromLong(1L);
exit:
#ifdef _WIN32
    if (cookie != NULL) {
        RemoveDllDirectory(cookie);
    }
    if (dir_buffer != NULL) {
        PyMem_RawFree(dir_buffer);
    }
    if (wide_path != NULL) {
        PyMem_Free(wide_path);
    }
#else
    if (bytes_path != NULL) {
        Py_DECREF(bytes_path);
    }
#endif
    if (unicode_path != NULL) {
        Py_DECREF(unicode_path);
    }
    return result;
}

static PyMethodDef bootstrap_methods[] = {
    {
        "load",
        (PyCFunction)bootstrap_load,
        METH_VARARGS | METH_KEYWORDS,
        PyDoc_STR("Load a libsixel shared library using platform specific APIs."),
    },
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef bootstrap_module = {
    PyModuleDef_HEAD_INIT,
    "_bootstrap",
    "Internal helpers for preloading libsixel binaries.",
    0,
    bootstrap_methods,
    NULL,
    NULL,
    NULL,
    NULL
};

PyMODINIT_FUNC
PyInit__bootstrap(void)
{
    return PyModule_Create(&bootstrap_module);
}
