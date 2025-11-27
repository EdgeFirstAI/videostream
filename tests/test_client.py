import pytest
from videostream import Client


def test_client():
    """
    Testing some basic client setup code.  The main tests are in test_ipc which
    require both the host and client to be setup for communications.
    """
    with pytest.raises(Exception):
        Client()
    with pytest.raises(Exception):
        Client(None)
    with pytest.raises(Exception):
        Client('')
