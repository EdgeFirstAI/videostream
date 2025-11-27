import pytest
from videostream import Client, Host, Frame
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
