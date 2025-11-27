import pytest
import videostream as vsl


def test_version():
    assert type(vsl.version()) == str
