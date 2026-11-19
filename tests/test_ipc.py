import os
import sys
import time
import pytest
from videostream import Client, Host, Frame
from videostream.library import lib
from pathlib import Path


def test_connect():
    """
    Test basic connection support between host and client.  Tests need to be
    careful to ensure host is ready for client otherwise the client will block
    waiting for the host to be ready, which will never happen.  Later tests add
    threading to the mix to better orchestrate the communications tests.
    """
    h = Host('/tmp/test_connect.vsl')
    assert Path(h.path).exists()
    c = Client(h.path)
    assert c.path == h.path

    h.process()
    assert h.clients_count == 1

    c.disconnect()
    h.process()
    assert h.clients_count == 0


def test_post():
    """
    """
    h = Host('/tmp/test_post.vsl')
    assert Path(h.path).exists()
    c = Client(h.path)
    assert c.path == h.path

    h.process()
    assert h.clients_count == 1

    f1 = Frame(640, 480, fourcc='RGB3')
    f1.alloc()
    assert f1.size == 640 * 480 * 3
    f1_handle = f1.handle

    h.post(f1)
    assert f1._ptr is None
    with pytest.raises(Exception):
        f1.handle
        f1.width
        f1.height
        f1.fourcc

    f2 = c.frame_wait()
    assert f2 is not None
    assert f2._ptr is not None
    assert f2.width == 640
    assert f2.height == 480
    assert f1_handle != f2.handle


def _socket_inodes():
    """Socket inodes of the descriptors this process has open."""
    inodes = set()
    for fd in os.listdir('/proc/self/fd'):
        try:
            target = os.readlink('/proc/self/fd/%s' % fd)
        except OSError:
            continue
        if target.startswith('socket:'):
            inodes.add(target)
    return inodes


@pytest.mark.skipif(not sys.platform.startswith('linux'),
                    reason='reads /proc/self/fd')
def test_wait_timeout_keeps_connection():
    """
    A frame_wait that times out must leave the client's connection alone.
    The client watchdog used to close the socket when a short timeout expired,
    without the client lock, so the next wait reconnected.  On clients that
    wait and unlock from several threads that close raced the reconnect and
    leaked a socket pair on the client and the host each time.
    """
    h = Host('/tmp/test_wait_timeout.vsl')
    c = Client(h.path)
    h.process()
    assert h.clients_count == 1

    lib.vsl_client_set_timeout(c._ptr, 0.05)
    before = _socket_inodes()

    for _ in range(20):
        with pytest.raises(RuntimeError):
            c.frame_wait()
        time.sleep(0.02)
        h.process()

    assert h.clients_count == 1
    assert _socket_inodes() == before

