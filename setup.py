# Copyright 2023 by Au-Zone Technologies.  All Rights Reserved.
#
# Unauthorized copying of this file, via any medium is strictly prohibited
# Proprietary and confidential.
#
# This source code is provided solely for runtime interpretation by Python.
# Modifying or copying any source code is explicitly forbidden.

from re import sub
from subprocess import Popen, PIPE
from setuptools import setup


def get_version():
    """
    Returns project version as string from 'git describe' command.
    """
    pipe = Popen('git describe --tags --always', stdout=PIPE, shell=True)
    version = str(pipe.communicate()[0].rstrip().decode("utf-8"))
    return str(sub(r'-g\w+', '', version))


setup(
    name='videostream',
    version=get_version(),
    description='DeepView VideoStream for Python',
    author='Au-Zone Technologies',
    author_email='support@au-zone.com',
    license='PROPRIETARY',
    url='https://embeddedml.com',
    packages=[
        'deepview.videostream',
    ],
    install_requires=[],
    extras_require={},
    entry_points={},
)
