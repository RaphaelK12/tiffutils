/*
 * Copyright (c) 2013, North Carolina State University Aerial Robotics Club
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the North Carolina State University Aerial Robotics Club
 *       nor the names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <Python.h>
#include <tiffio.h>

#ifndef TIFFTAG_CFAREPEATPATTERNDIM
#error libtiff with CFA pattern support required
#endif

#define PY_ARRAY_UNIQUE_SYMBOL  tiffutils_core_ARRAY_API
#include <numpy/arrayobject.h>

enum tiff_cfa_color {
    CFA_RED = 0,
    CFA_GREEN = 1,
    CFA_BLUE = 2,
};

enum cfa_pattern {
    CFA_BGGR = 0,
    CFA_GBRG,
    CFA_GRBG,
    CFA_RGGB,
    CFA_NUM_PATTERNS,
};

static const char cfa_patterns[4][CFA_NUM_PATTERNS] = {
    [CFA_BGGR] = {CFA_BLUE, CFA_GREEN, CFA_GREEN, CFA_RED},
    [CFA_GBRG] = {CFA_GREEN, CFA_BLUE, CFA_RED, CFA_GREEN},
    [CFA_GRBG] = {CFA_GREEN, CFA_RED, CFA_BLUE, CFA_GREEN},
    [CFA_RGGB] = {CFA_RED, CFA_GREEN, CFA_GREEN, CFA_BLUE},
};

/* Default ColorMatrix1, when none provided */
static const float default_color_matrix1[] = {
     2.005, -0.771, -0.269,
    -0.752,  1.688,  0.064,
    -0.149,  0.283,  0.745
};

/*
 * Create ColorMatrix1 array
 *
 * Create a color matrix array from list.  If no list provided, use the
 * default ColorMatrix1.
 *
 * @param list  Python list containing color matrix
 * @param color_matrix1 Pointer to destination array for color matrix
 * @param len   Array size returned here
 * @returns 0 on success, negative on error, with exception set
 */
static int handle_color_matrix1(PyObject *list, float **color_matrix1, int *len) {
    /* No list provided, use default */
    if (list == Py_None) {
        *color_matrix1 = malloc(sizeof(default_color_matrix1));
        if (!*color_matrix1) {
            PyErr_SetString(PyExc_MemoryError, "Unable to allocate color matrix");
            return -1;
        }

        memcpy(*color_matrix1, default_color_matrix1,
               sizeof(default_color_matrix1));

        *len = sizeof(default_color_matrix1)/sizeof(float);

        return 0;
    }

    if (!PyList_Check(list)) {
        return -1;
    }

    *len = PyList_Size(list);

    *color_matrix1 = malloc(*len*sizeof(float));
    if (!*color_matrix1) {
        PyErr_SetString(PyExc_MemoryError, "Unable to allocate color matrix");
        return -1;
    }

    for (int i = 0; i < *len; i++) {
        PyObject *item = PyList_GetItem(list, i);
        if (!item) {
            free(*color_matrix1);
            return -1;
        }

        (*color_matrix1)[i] = (float) PyFloat_AsDouble(item);
        if (PyErr_Occurred()) {
            free(*color_matrix1);
            return -1;
        }
    }

    return 0;
}

static PyObject *tiffutils_save_dng(PyObject *self, PyObject *args, PyObject *kwds) {
    static char *kwlist[] = {
        "image", "filename", "camera", "cfa_pattern", "color_matrix1", NULL
    };

    PyArrayObject *array;
    PyObject *color_matrix1_list = Py_None;
    unsigned int pattern = CFA_RGGB;
    float *color_matrix1;
    int color_matrix1_len;
    int ndims, width, height, type, bytes_per_pixel;
    npy_intp *dims;
    char *filename;
    char *camera = "Unknown";
    char *mem;
    TIFF *file = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "Os|sIO", kwlist, &array,
                                     &filename, &camera, &pattern,
                                     &color_matrix1_list)) {
        return NULL;
    }

    if (pattern >= CFA_NUM_PATTERNS) {
        PyErr_SetString(PyExc_ValueError, "Invalid CFA pattern");
        return NULL;
    }

    if (!PyArray_Check(array)) {
        PyErr_SetString(PyExc_TypeError, "nparray required");
        return NULL;
    }

    if (!PyArray_ISCONTIGUOUS(array)) {
        PyErr_SetString(PyExc_TypeError, "nparray must be contiguous");
        return NULL;
    }

    ndims = PyArray_NDIM(array);
    dims = PyArray_DIMS(array);
    type = PyArray_TYPE(array);
    mem = PyArray_BYTES(array);

    if (ndims != 2) {
        PyErr_SetString(PyExc_TypeError, "nparray must be 2 dimensional");
        return NULL;
    }

    height = dims[0];
    width = dims[1];

    switch (type) {
    case NPY_UINT8:
        bytes_per_pixel = 1;
        break;
    case NPY_UINT16:
        bytes_per_pixel = 2;
        break;
    default:
        PyErr_SetString(PyExc_TypeError, "nparray must be uint8 or uint16");
        return NULL;
    }

    if (handle_color_matrix1(color_matrix1_list, &color_matrix1,
                             &color_matrix1_len)) {
        return NULL;
    }

    file = TIFFOpen(filename, "w");
    if (file == NULL) {
        PyErr_SetString(PyExc_IOError, "libtiff failed to open file for writing.");
        return NULL;
    }

    TIFFSetField(file, TIFFTAG_IMAGEWIDTH, width);
    TIFFSetField(file, TIFFTAG_IMAGELENGTH, height);
    TIFFSetField(file, TIFFTAG_UNIQUECAMERAMODEL, camera);

    TIFFSetField(file, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    TIFFSetField(file, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(file, TIFFTAG_SUBFILETYPE, 0);

    TIFFSetField(file, TIFFTAG_BITSPERSAMPLE, 8*bytes_per_pixel);
    TIFFSetField(file, TIFFTAG_SAMPLESPERPIXEL, 1);
    TIFFSetField(file, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_CFA);

    TIFFSetField(file, TIFFTAG_CFAREPEATPATTERNDIM, (short[]){2,2});
    TIFFSetField(file, TIFFTAG_CFAPATTERN, cfa_patterns[pattern]);
    TIFFSetField(file, TIFFTAG_COLORMATRIX1, color_matrix1_len, color_matrix1);
    TIFFSetField(file, TIFFTAG_DNGVERSION, "\001\001\0\0");
    TIFFSetField(file, TIFFTAG_DNGBACKWARDVERSION, "\001\0\0\0");

    for (int row = 0; row < height; row++) {
        if (TIFFWriteScanline(file, mem, row, 0) < 0) {
            TIFFClose(file);
            PyErr_SetString(PyExc_IOError, "libtiff failed to write row.");
            return NULL;
        }
        else {
            mem += width * bytes_per_pixel;
        }
    }

    TIFFWriteDirectory(file);
    TIFFClose(file);

    free(color_matrix1);

    Py_INCREF(Py_None);
    return Py_None;
}

PyMethodDef tiffutilsMethods[] = {
    {"save_dng", (PyCFunction) tiffutils_save_dng, METH_VARARGS | METH_KEYWORDS,
        "save_dng(image, filename, [camera='Unknown', cfa_pattern=tiffutils.CFA_RGGB,\n"
        "   color_matrix1=None])\n\n"
        "Save an nparray as a DNG.\n\n"
        "The image will be saved as a RAW DNG,a superset of TIFF.\n"
        "Arguments:\n"
        "    image: Image to save.  This should be a 2-dimensional, uint8 or\n"
        "        uint16 Numpy array.\n"
        "    filename: Destination file to save DNG to.\n"
        "    camera: Unique name of camera model\n"
        "    cfa_pattern: Bayer color filter array pattern.\n"
        "       One of tiffutils.CFA_*\n"
        "    color_matrix1: A list containing the desired ColorMatrix1.\n"
        "       If not specified, a default is used.\n\n"
        "Raises:\n"
        "    TypeError: image is not the appropriate format.\n"
        "    IOError: file could not be written."
    },
    {NULL, NULL, 0, NULL}
};

#if PY_MAJOR_VERSION >= 3
static struct PyModuleDef tiffutilsmodule = {
    PyModuleDef_HEAD_INIT,
    "tiffutils",    /* name of module */
    NULL, /* module documentation, may be NULL */
    -1,       /* size of per-interpreter state of the module,
                or -1 if the module keeps state in global variables. */
    tiffutilsMethods
};
#endif

#if PY_MAJOR_VERSION >= 3
PyMODINIT_FUNC PyInit_tiffutils(void) {
#else
PyMODINIT_FUNC inittiffutils(void) {
#endif
    PyObject* m;

    import_array();

    #if PY_MAJOR_VERSION >= 3
    m = PyModule_Create(&tiffutilsmodule);
    #else
    m = Py_InitModule("tiffutils", tiffutilsMethods);
    #endif

    if (m == NULL) {
        #if PY_MAJOR_VERSION >= 3
        return NULL;
        #else
        return;
        #endif
    }

    PyModule_AddIntConstant(m, "CFA_BGGR", CFA_BGGR);
    PyModule_AddIntConstant(m, "CFA_GBRG", CFA_GBRG);
    PyModule_AddIntConstant(m, "CFA_GRBG", CFA_GRBG);
    PyModule_AddIntConstant(m, "CFA_RGGB", CFA_RGGB);

    #if PY_MAJOR_VERSION >= 3
    return m;
    #endif
}

int main(int argc, char *argv[]) {
    #if PY_MAJOR_VERSION >= 3
    wchar_t name[128];
    mbstowcs(name, argv[0], 128);
    #else
    char name[128];
    strncpy(name, argv[0], 128);
    #endif

    /* Pass argv[0] to the Python interpreter */
    Py_SetProgramName(name);

    /* Initialize the Python interpreter.  Required. */
    Py_Initialize();

    /* Add a static module */
    #if PY_MAJOR_VERSION >= 3
    PyInit_tiffutils();
    #else
    inittiffutils();
    #endif

    return 0;
}