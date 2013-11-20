tiffutils
=========

Various TIFF utility functions.

This module provide various functionality for working with TIFF files that
is not available in other modules.

Currently, the only function allows saving RAW Bayer images as a DNG file.

## Requirements

* python-dev
    * This is a Python C API module, so the Python headers are required
    * Python 2 and Python 3 are supported
    * Tested on Python 2.7 and Python 3.2
* python-numpy-dev
    * Tested on Numpy 1.6
* libtiff-dev > 4.0.3
    * libtiff with support for CFA (color filter array) tags is required
    * Support is merged into the libtiff trunk, and will be released with
      the next version of libtiff.
    * A patch for libtiff 4.0.3 is included as libtiff-4.0.3.patch
    * The latest version can be downloaded from the
      [libtiff website](http://www.remotesensing.org/libtiff/).

## Building and installing

First ensure, that you have libtiff > 4.0.3.  If not, you need to configure,
build, and install a newer version.  The CVS trunk already includes the required
CFA tag support, so it can be used directly. Otherwise, download libtiff 4.0.3,
apply the included patch, and configure, build, and install libtiff.

    $ cd /path/to/libtiff
    $ patch -p1 < /path/to/tiffutils/libtiff-4.0.3.patch

Once all dependencies are met, it is simple to build and install the module:

    $ python setup.py install

Or, for Python 3:

    $ python3 setup.py install

Of course, if the installation location requires root permissions, `sudo` may
be necessary.