Python Audiotools
=================

This package contains a Python 3 wrapper around the libav library. This
is work in progress. The only things which are currently supported are
reading and setting (not writing) metadata.


Requirements
------------

- python3.x (Tested using python3.3)
- libavcodec
- libavformat
- libavutil


Installation
------------

Using pip: ::

    pip install audiotools

From GitHub: ::

    git clone ...
    cd audiotools
    ./setup.py install


Usage
-----

Reading song metadata:

>>> from audiotools import Song
>>> filename = 'Finntroll - Trollhammaren.flac'
>>> song = Song(filename)
>>> song['artist']
Finntroll
>>> song['album']
Trollhammaren
>>> song['title']
Trollhammaren

Saving song metadata (not implemented):

>>> song['album'] = 'Nattfödd'
>>> song['album']
'Nattfödd'
>>> song.save()

Converting the song (not implemented):

>>> converted = song.convert('Finntroll - Trollhammaren.ogg',
...                          format='opus', q=4)
>>> converted.filename
Finntroll - Trollhammaren.ogg

Playback (not implemented):

>>> song.play()
0.00
>>> song.pause()
4.29
>>> song.play_or_pause()
4.29
>>> song.playing_time
12.53


Testing
-------

Run test/test_audiotools.py: ::

    ./setup.py test


Coding style
------------

All Python code must be in compliance with pep8_. C code must be in compliance
with pep7_. All code must be covered by unittests.

.. _pep7: http://www.python.org/dev/peps/pep-0007/
.. _pep8: http://www.python.org/dev/peps/pep-0008/
