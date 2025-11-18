# Copyright 2022 by Au-Zone Technologies.  All Rights Reserved.
#
# Unauthorized copying of this file, via any medium is strictly prohibited
# Proprietary and confidential.
#
# This source code is provided solely for runtime interpretation by Python.
# Modifying or copying any source code is explicitly forbidden.

from ctypes import \
    byref, c_int, c_int64, c_size_t, c_void_p, py_object, pythonapi
from enum import IntEnum
from pathlib import Path
from typing import Optional, Union
from typeguard import typechecked
from .library import lib, errmsg
from .frame import Frame


@typechecked
class Client:
    """
    The VideoStream client connects to a host in order to receive frames which
    have been published on the host-side.  A client can connect to a single
    host but a host can have many client connections.

    Captured frames can be used as normal frames though with some semantic
    differences.  The underlying buffer should not be changed by calling alloc
    nor dealloc as doing so will release the frame back to the host and would
    be equivalent to creating a new frame with similar parameters but without
    the underlying buffer and therefore without the contents.

    Frames have a limited timespan controlled by the host, the default being
    100ms.  The local frame object remains valid beyond this time but attempts
    to lock or map after expiration will raise an error.  Locking a frame stops
    the frame from expiring and allows software which needs more time to read
    and process the frame to use as much time as they need.

    NOTE: Memory is a limited resource and this is especially true for DMA
    memory.  Locking too many frames will lead to exhausted resources.  Make
    sure to only lock as required and return frames once they are no longer
    needed!

    """

    def __init__(self, path: Union[str, Path], reconnect: bool = False):
        if isinstance(path, Path):
            path = path.as_posix()
        self._ptr = lib.vsl_client_init(path.encode('utf-8'), None, reconnect)
        if self._ptr is None:
            raise RuntimeError('Failed to create host: %s' % errmsg())

    def __del__(self):
        if hasattr(self, '_ptr') and self._ptr is not None:
            lib.vsl_client_release(self._ptr)
            self._ptr = None

    @property
    def path(self) -> str:
        p = lib.vsl_client_path(self._ptr)
        if p is None:
            raise RuntimeError('Failed to read host path: %s' % errmsg())
        return p.decode('utf-8')

    def disconnect(self):
        lib.vsl_client_disconnect(self._ptr)

    def frame_wait(self, until: int = 0) -> Frame:
        frame_ptr = lib.vsl_frame_wait(self._ptr, until)
        if frame_ptr is None:
            raise RuntimeError('Failed receive frame: %s' % errmsg())
        return Frame(0, 0, 0, ptr=frame_ptr)
