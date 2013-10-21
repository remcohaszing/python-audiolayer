#!/usr/bin/env python3

from setuptools import Extension
from setuptools import setup


with open('README.rst') as f:
    readme = f.read()


setup(name='audiotools',
      version='0.1.0',
      author='Remco Haszing',
      author_email='remcohaszing@gmail.com',
      url='example.com',
      description='This module provides a wrapper around the libav C library',
      long_description=readme,
#      download_url=,
      license='WTFPL',
      test_suite='test',
      ext_modules=[Extension('audiotools',
                             sources=['src/audiotoolsmodule.c'],
                             libraries=['avformat', 'avutil'])])
