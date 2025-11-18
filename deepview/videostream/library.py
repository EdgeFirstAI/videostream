# Copyright 2022 by Au-Zone Technologies.  All Rights Reserved.
#
# Unauthorized copying of this file, via any medium is strictly prohibited
# Proprietary and confidential.
#
# This source code is provided solely for runtime interpretation by Python.
# Modifying or copying any source code is explicitly forbidden.

from ctypes import \
    CDLL, POINTER, get_errno, \
    c_char_p, c_void_p, c_size_t, c_ssize_t, \
    c_int, c_int64, c_uint32, c_bool, c_float
from os import environ, strerror
from os.path import isdir, join
from typing import Optional
from typeguard import typechecked


@typechecked
def version() -> str:
    """
    Version of the underlying videostream.ext library, the .ext varies across
    platforms.

        * Windows: ``VideoStream.dll``
        * Linux: ``libvideostream.so``
        * MacOS: ``libvideostream.dylib``

    Returns:
        version string as major.minor.patch-extra format.
    """
    string = lib.vsl_version()
    return c_char_p(string).value.decode('utf-8')


@typechecked
def timestamp() -> int:
    """
    Returns the current VSL timestamp in nanoseconds.
    """
    return lib.vsl_timestamp()


@typechecked
def errmsg() -> Optional[str]:
    """
    Returns the error message for the current system error code (errno).
    """
    return strerror(get_errno())


@typechecked
def load_library(libname: Optional[str] = None) -> CDLL:
    """
    Internal function used to load and configure the VideoStream library. This
    function should not be called directly but instead is called automatically
    when the deepview.videostream library is first loaded.

    The environment variable ``VIDEOSTREAM_LIBRARY`` can be used to point to
    the location of ``videostream.dll/dylib/so`` for cases where it cannot be
    found.

    Note:
        The library is not part of the Python package but is installed
        separately, typically as part of DeepView VisionPack installations.

    Returns:
        A ctypes.CDLL object containing the VideoStream library.

    Raises:
        :py:class:`EnvironmentError`: if the VideoStream library cannot be
        located.
    """
    if 'VIDEOSTREAM_LIBRARY' in environ:
        libname = environ['VIDEOSTREAM_LIBRARY']

    if libname is not None:
        if isdir(libname) and libname.endswith('.framework'):
            return CDLL(
                join(libname, 'Versions', 'Current', 'VideoStream'))
        else:
            return CDLL(libname)
    else:
        try:
            return CDLL('VideoStream.dll')
        except OSError:
            pass

        try:
            return CDLL('./VideoStream.dll')
        except OSError:
            pass

        try:
            return CDLL('libvideostream.so')
        except OSError:
            pass

        try:
            return CDLL('libvideostream.dylib')
        except OSError:
            pass

    raise EnvironmentError(
        'Unable to load the VideoStream library.  Try setting the environment \
         variable VIDEOSTREAM_LIBRARY to the VideoStream library.')


@typechecked
def load_symbols(lib: CDLL):
    """
    Loads the symbols from the VideoStream library into the `lib` object to be
    used by the various Python API.

    These symbols are documented in videostream.h and the VideoStream Library
    User Manual.

    Args:
        lib: Library object returned from :func:`load_library()`
    """

    # Core Library API
    lib.vsl_version.argtypes = []
    lib.vsl_version.restype = c_char_p

    lib.vsl_timestamp.argtypes = []
    lib.vsl_timestamp.restype = c_int64

    lib.vsl_fourcc_from_string.argtypes = [c_char_p]
    lib.vsl_fourcc_from_string.restype = c_uint32

    # Frame API
    lib.vsl_frame_init.argtypes = [
        c_uint32, c_uint32, c_uint32, c_uint32, c_void_p, c_void_p]
    lib.vsl_frame_init.restype = c_void_p

    lib.vsl_frame_release.argtypes = [c_void_p]
    lib.vsl_frame_release.restype = None

    lib.vsl_frame_width.argtypes = [c_void_p]
    lib.vsl_frame_width.restype = c_int

    lib.vsl_frame_height.argtypes = [c_void_p]
    lib.vsl_frame_height.restype = c_int

    lib.vsl_frame_stride.argtypes = [c_void_p]
    lib.vsl_frame_stride.restype = c_int

    lib.vsl_frame_size.argtypes = [c_void_p]
    lib.vsl_frame_size.restype = c_int

    lib.vsl_frame_fourcc.argtypes = [c_void_p]
    lib.vsl_frame_fourcc.restype = c_uint32

    lib.vsl_frame_path.argtypes = [c_void_p]
    lib.vsl_frame_path.restype = c_char_p

    lib.vsl_frame_handle.argtypes = [c_void_p]
    lib.vsl_frame_handle.restype = c_int

    lib.vsl_frame_paddr.argtypes = [c_void_p]
    lib.vsl_frame_paddr.restype = c_ssize_t

    lib.vsl_frame_attach.argtypes = [c_void_p, c_int, c_size_t, c_size_t]
    lib.vsl_frame_attach.restype = c_int

    lib.vsl_frame_alloc.argtypes = [c_void_p, c_char_p]
    lib.vsl_frame_alloc.restype = c_int

    lib.vsl_frame_unalloc.argtypes = [c_void_p]
    lib.vsl_frame_unalloc.restype = None

    lib.vsl_frame_mmap.argtypes = [c_void_p, POINTER(c_size_t)]
    lib.vsl_frame_mmap.restype = c_void_p

    lib.vsl_frame_munmap.argtypes = [c_void_p]
    lib.vsl_frame_munmap.restype = None

    lib.vsl_frame_sync.argtypes = [c_void_p, c_int, c_int]
    lib.vsl_frame_sync.restype = c_int

    # Host API
    lib.vsl_host_init.argtypes = [c_char_p]
    lib.vsl_host_init.restype = c_void_p

    lib.vsl_host_release.argtypes = [c_void_p]
    lib.vsl_host_release.restype = None

    lib.vsl_host_path.argtypes = [c_void_p]
    lib.vsl_host_path.restype = c_char_p

    lib.vsl_host_poll.argtypes = [c_void_p, c_int64]
    lib.vsl_host_poll.restype = c_int

    lib.vsl_host_process.argtypes = [c_void_p]
    lib.vsl_host_process.restype = c_int

    lib.vsl_host_sockets.argtypes = [
        c_void_p, c_size_t, POINTER(c_int), POINTER(c_size_t)]
    lib.vsl_host_sockets.restype = c_int

    lib.vsl_host_post.argtypes = [
        c_void_p, c_void_p, c_int64, c_int64, c_int64, c_int64]
    lib.vsl_host_post.restype = c_int

    # Client API
    lib.vsl_client_init.argtypes = [c_char_p, c_void_p, c_bool]
    lib.vsl_client_init.restype = c_void_p

    lib.vsl_client_release.argtypes = [c_void_p]
    lib.vsl_client_release.restype = None

    lib.vsl_client_disconnect.argtypes = [c_void_p]
    lib.vsl_client_disconnect.restype = None

    lib.vsl_client_userptr.argtypes = [c_void_p]
    lib.vsl_client_userptr.restype = c_void_p

    lib.vsl_client_path.argtypes = [c_void_p]
    lib.vsl_client_path.restype = c_char_p

    lib.vsl_client_set_timeout.argtypes = [c_void_p, c_float]
    lib.vsl_client_set_timeout.restype = None

    lib.vsl_frame_wait.argtypes = [c_void_p, c_int64]
    lib.vsl_frame_wait.restype = c_void_p


if 'lib' not in locals():
    lib = load_library()
    load_symbols(lib)
