Python Audiotools
=================

This package contains a Python 3 Extension for playing audio files, reading and
writing metadata and converting those files. For decoding files the libav_
library is used. Playback is handled by libportaudio_.

This is work in progress. The only things which are currently supported are
reading and setting (not writing) metadata. The project website can be found at
`GitHub
<https://github.com/RemcoHaszing/python-audiotools>`_.


Requirements
------------

- python3.x (Tested using python3.3)
- libavcodec
- libavformat
- libavutil
- libportaudio


Installation
------------

Using pip: (Not yet implemented)::

    pip install audiotools

From GitHub::

    git clone https://github.com/RemcoHaszing/python-audiotools.git
    cd python-audiotools
    python3 setup.py install


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

Playback (current implementation):

>>> song.play()  # Wait for the song to finish.
>>>

Playback (target implementation):

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

The unittests can be run using::

    python3 setup.py test

It is also possible to use nose_ to run the tests::

    python3 setup.py nosetests


Coding style
------------

- C code must be in compliance with pep7_.
- Python code must be in compliance with pep8_.
- All code must be covered by unittests.

To test the Python code for pep8 compliance, go to the project root directory
and run::

    python3 setup.py flake8


License
-------

This project is licensed under the BSD 3-Clause License.


.. _libav: https://libav.org
.. _libportaudio: http://portaudio.com/
.. _nose: http://nose.readthedocs.org
.. _pep7: http://www.python.org/dev/peps/pep-0007
.. _pep8: http://www.python.org/dev/peps/pep-0008
