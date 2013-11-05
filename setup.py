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
    version='0.1.1',
    author='Remco Haszing',
    author_email='remcohaszing@gmail.com',
    maintainer='Remco Haszing',
    maintainer_email='remcohaszing@gmail.com',
    url='https://github.com/remcohaszing/python-audiolayer',
    description='This module provides a wrapper around the libav C library',
    long_description=readme,
    download_url='https://pypi.python.org/packages/source/'
                 'a/audiolayer/audiolayer-0.1.1.tar.gz',
    license='BSD 3-Clause License',
    test_suite='test',
    classifiers=[
        'Development Status :: 3 - Alpha',
        'License :: OSI Approved :: BSD License',
        'Operating System :: POSIX',
        'Programming Language :: C',
        'Programming Language :: Python :: 3.3',
        'Topic :: Multimedia :: Sound/Audio'],
    ext_modules=[Extension(
        'audiolayer',
        sources=['src/audiolayermodule.c'],
        libraries=c_libs)])
