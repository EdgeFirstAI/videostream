# Copyright 2022 by Au-Zone Technologies.  All Rights Reserved.
#
# Unauthorized copying of this file, via any medium is strictly prohibited
# Proprietary and confidential.
#
# This source code is provided solely for runtime interpretation by Python.
# Modifying or copying any source code is explicitly forbidden.

from ctypes import \
    byref, c_int, c_int64, c_size_t, c_void_p, py_object, pythonapi
from deepview.videostream.library import lib, errmsg
from enum import IntEnum
from pathlib import Path
from typing import Optional, Union
from typeguard import typechecked
from .frame import Frame


@typechecked
class Host:
    """
    The VideoStream host allows sharing of frames to clients.  Frames created
    by the host can be posted and connected clients can receive them.  The
    sharing is done through a UNIX Domain Socket but only the frame information
    is transmitted with the descriptor for the underlying buffer being shared
    through the anciliary channel.  This allows for zero-copy frame sharing
    between the host and clients.
    """

    def __init__(self, path: Union[str, Path]):
        if isinstance(path, Path):
            path = path.as_posix()
        self._ptr = lib.vsl_host_init(path.encode('utf-8'))
        if self._ptr is None:
            raise RuntimeError('Failed to create host: %s' % errmsg())

    def __del__(self):
        if hasattr(self, '_ptr') and self._ptr is not None:
            lib.vsl_host_release(self._ptr)
            self._ptr = None

    @property
    def path(self) -> str:
        p = lib.vsl_host_path(self._ptr)
        if p is None:
            raise RuntimeError('Failed to read host path: %s' % errmsg())
        return p.decode('utf-8')

    @property
    def listener(self) -> int:
        s = c_int()
        ret = lib.vsl_host_sockets(self._ptr, c_size_t(1), byref(s), None)
        if ret == -1:
            raise RuntimeError(
                'Failed to get listener socket: %s' % errmsg())
        return s.value

    @property
    def clients_count(self) -> int:
        """
        Returns the number of clients connected to the host.
        """
        n = c_size_t()
        ret = lib.vsl_host_sockets(self._ptr, 0, None, byref(n))
        if ret == -1:
            raise RuntimeError(
                'Failed to get client sockets: %s' % errmsg())
        # Account for the listener socket which will always be present.
        return n.value - 1

    def poll(self, timeout: float = 0.0) -> int:
        """
        Polls all host sockets, including the listener, for readiness.
        """
        t = c_int64(int(timeout * 1000))
        ret = lib.vsl_host_poll(self._ptr, t)
        if ret == -1:
            raise RuntimeError('Failed to poll host socket: %s' % errmsg())
        return ret

    def process(self) -> None:
        """
        Process host sockets, accepting new clients and servicing connected
        clients.
        """
        ret = lib.vsl_host_process(self._ptr)
        if ret == -1:
            raise RuntimeError('Failed to process host: %s' % errmsg())

    def post(self,
             frame: Frame,
             expires: int = 0,
             duration: int = 0,
             pts: int = 0,
             dts: int = 0) -> None:
        ret = lib.vsl_host_post(self._ptr, frame._ptr,
                                expires, duration, pts, dts)
        if ret == -1:
            raise RuntimeError('Failed to post frame: %s' % errmsg())
        frame._ptr = None
        del frame
