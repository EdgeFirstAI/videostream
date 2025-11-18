import pytest
import deepview.videostream as vsl


def test_version():
    assert type(vsl.version()) == str
