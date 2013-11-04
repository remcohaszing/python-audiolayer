#!/usr/bin/env python3

"""
Setup file for the audiolayer module.

"""
from setuptools import Extension
from setuptools import setup


with open('README.rst') as f:
    readme = f.read()


# Reading, writing, decoding and encoding audio files.
libav = ['avformat', 'avutil']

# Playback
libportaudio = ['portaudio']

c_libs = libav + libportaudio


setup(
    name='audiolayer',
    version='0.1.0',
    author='Remco Haszing',
    author_email='remcohaszing@gmail.com',
    maintainer='Remco Haszing',
    maintainer_email='remcohaszing@gmail.com',
    url='https://github.com/RemcoHaszing/python-audiolayer',
    description='This module provides a wrapper around the libav C library',
    long_description=readme,
    download_url=
        'https://github.com/RemcoHaszing/python-audiolayer/archive/master.zip',
    license='BSD 3-Clause License',
    test_suite='test',
    ext_modules=[Extension(
        'audiolayer',
        sources=['src/audiolayermodule.c'],
        libraries=c_libs)])
