# Copyright 2022 by Au-Zone Technologies.  All Rights Reserved.
#
# Unauthorized copying of this file, via any medium is strictly prohibited
# Proprietary and confidential.
#
# This source code is provided solely for runtime interpretation by Python.
# Modifying or copying any source code is explicitly forbidden.

from ctypes import byref, c_int, c_size_t, c_void_p, py_object, pythonapi
from deepview.videostream.library import lib
from enum import IntEnum
from typing import Optional
from typeguard import typechecked


@typechecked
class Frame:
    """
    VideoStream Frame class manages frames.
    """

    @typechecked
    class Sync(IntEnum):
        """
        When manually controlling DMA sync these definitions are used to state
        whether the sync is for read-only, write-only, or read-write operation.
        """
        RDONLY = 0
        WRONLY = 1
        RW = 2

    def __init__(self, width, height, fourcc, stride=0, ptr=None):
        """
        Constructs the frame for the provided width, height, and fourcc.
        """
        if ptr is not None:
            self._ptr = ptr
            return
        if type(fourcc) == str:
            fourcc_ = lib.vsl_fourcc_from_string(fourcc.encode('utf-8'))
            if fourcc_ == 0:
                raise RuntimeError('Invalid fourcc %s' % fourcc)
            fourcc = fourcc_
        self._ptr = lib.vsl_frame_init(
            width, height, stride, fourcc, None, None)
        if self._ptr is None:
            raise RuntimeError('Failed to create Frame')

    def __del__(self):
        if hasattr(self, '_ptr') and self._ptr is not None:
            lib.vsl_frame_release(self._ptr)
            self._ptr = None

    @property
    def width(self) -> int:
        """
        Returns the width of the frame in pixels.
        """
        return lib.vsl_frame_width(self._ptr)

    @property
    def height(self) -> int:
        """
        Returns the height of the frame in pixels.
        """
        return lib.vsl_frame_height(self._ptr)

    @property
    def stride(self) -> int:
        """
        Returns the number of bytes to the next row.  This value is not the
        width but rather the width * pixel_size + padding; where pixel_size
        refers to the number of bytes for a major pixel, and padding is extra
        area before the next row.

        Major pixel has varying meaning.  For RGB type images a pixel is the
        combination of R,G,B, and possibly A or X (X means ignored).  An RGB
        image could also be called RGB888 which refers to 3 8-bit pixels. For
        YUV type images it gets a little more complicated.  For packed formats
        such as YUYV the pixel stride is 2 (16bits per pixel) as the pixels are
        an alternating pattern of YU/YV, this also implies the resolution must
        be a factor of 2.  For planar formats such as I420 or NV12 the YUV
        planes are separated, differently.  I420 has a plane for Y and a plane
        for UV (packed).  NV12 has a plane for each of Y, U, and V.  There is
        also some discrepancy in interpretation between vendors, our choice is
        based on the underlying hardware/software/drivers used by videostream.

        Padding is commonly used for cropping, by adjusting the starting pixel
        you can choose the x,y starting coordinate from the left,top along with
        stride to overshoot the width of the image to align with the next x
        coordinate.
        """
        return lib.vsl_frame_stride(self._ptr)

    @property
    def fourcc(self) -> str:
        """
        Returns the FOURCC code as a string.
        """
        code = lib.vsl_frame_fourcc(self._ptr)
        return '%c%c%c%c' % (
            chr(code & 0xFF),
            chr((code >> 8) & 0xFF),
            chr((code >> 16) & 0xFF),
            chr((code >> 24) & 0xFF))

    @property
    def size(self) -> int:
        """
        Returns the size of the underlying framebuffer.
        """
        sz = lib.vsl_frame_size(self._ptr)
        if sz <= 0:
            raise RuntimeError('Frame has unknown size')
        return sz

    @property
    def paddr(self) -> int:
        """
        Returns the physical address of the underlying buffer.

        Note: this is only available on certain embedded platforms.
        """
        p = lib.vsl_frame_paddr(self._ptr)
        if p == -1:
            raise RuntimeError('Frame cannot produce physical address')
        return p

    @property
    def path(self) -> Optional[str]:
        """
        Returns the path of the underlying frame buffer, if it is known.
        """
        s = lib.vsl_frame_path(self._ptr)
        return None if s is None else s.decode('UTF-8')

    @property
    def handle(self) -> int:
        """
        Returns the file descriptor for the underlying frame buffer.
        """
        fd = lib.vsl_frame_handle(self._ptr)
        if fd == -1:
            raise RuntimeError('Frame does not have a known file handle')
        return fd

    def map(self) -> memoryview:
        """
        Maps the buffer and returns a memoryview of the object.

        DANGEROUS: Note that you are responsible for calling unmap after you
        are done with the memoryview and MUST ensure to never access the buffer
        once it has been unmapped.  If you cannot guarantee the safety of your
        code then you must instead make a copy of the memoryview then dispose
        and do the unmap operation.
        """
        siz = c_size_t()
        ptr = lib.vsl_frame_mmap(self._ptr, byref(siz))
        if ptr is None:
            raise RuntimeError('Failed to map frame')
        from_memory = pythonapi.PyMemoryView_FromMemory
        from_memory.argtypes = (c_void_p, c_int, c_int)
        from_memory.restype = py_object
        return from_memory(ptr, siz.value, 0x200)

    def unmap(self) -> None:
        """
        Release the frame mapping.  Once you call unmap the previous memoryview
        returned by map() is now invalid!
        """
        lib.vsl_frame_munmap(self._ptr)

    def sync(self, enable: bool, mode: Sync) -> None:
        """
        Cache synchronization session control for when using DMA-backed
        buffers.

        This happens automatically on mmap/munmap but the API is also available
        for cases where the frame is updated in-place during a mapping.
        """
        ret = lib.vsl_frame_sync(self._ptr, 1 if enable else 0, mode.value)
        if ret == -1:
            raise RuntimeError('Failed to sync frame')

    def alloc(self, path=None):
        if path is not None:
            path = path.encode('UTF-8')
        ret = lib.vsl_frame_alloc(self._ptr, path)
        if ret == -1:
            raise RuntimeError('Failed to allocate frame')

    def unalloc(self):
        lib.vsl_frame_unalloc(self._ptr)

    def attach(self, fd, size=0, offset=0):
        ret = lib.vsl_frame_attach(self._ptr, fd, size, offset)
        if ret == -1:
            raise RuntimeError('Failed to attach buffer to frame')
