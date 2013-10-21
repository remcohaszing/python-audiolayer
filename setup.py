#!/usr/bin/env python3

from setuptools import Extension
from setuptools import setup


with open('README.rst') as f:
    readme = f.read()


setup(
    name='audiotools',
    version='0.1.0',
    author='Remco Haszing',
    author_email='remcohaszing@gmail.com',
    url='https://github.com/RemcoHaszing/python-audiotools',
    description='This module provides a wrapper around the libav C library',
    long_description=readme,
    download_url=
        'https://github.com/RemcoHaszing/python-audiotools/archive/master.zip',
    license='WTFPL',
    test_suite='test',
    ext_modules=[Extension(
        'audiotools',
        sources=['src/audiotoolsmodule.c'],
        libraries=['avformat', 'avutil'])])
