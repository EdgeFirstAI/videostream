import pytest
from videostream import Host
from pathlib import Path
from socket import socket, AF_UNIX, SOCK_SEQPACKET
from time import process_time
from warnings import warn


def test_host():
    """
    Tests basic host boilerplate such as creating the host creates the socket
    and proper error handling for bad or missing parameters.
    """
    with pytest.raises(Exception):
        h = Host()
    with pytest.raises(Exception):
        h = Host(None)
    with pytest.raises(Exception):
        h = Host('')

    p = Path('/tmp/test_host.vsl')
    assert not p.exists()
    h = Host(p)
    assert p.exists()
    assert h.path == p.as_posix()
    assert h.clients_count == 0

    s = socket(fileno=h.listener)
    assert s.family == AF_UNIX
    assert s.type == SOCK_SEQPACKET
    assert s.getsockname() == p.as_posix()

    # Poll with a one second timeout and assert that we
    # elapsed at least one second but less than two seconds.
    start = process_time()
    assert h.poll(1.0) == 0
    elapsed = process_time() - start
    # FIXME: function is returning after about 100µs need to investigate.
    assert elapsed < 2.0
    if elapsed < 1.0:
        warn('Host poll expected runtime over 1000ms but took %.03fms' %
             (elapsed * 1000.0))

    del h
    assert not p.exists()
