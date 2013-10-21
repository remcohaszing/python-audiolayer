#!/usr/bin/env python3

import os
import unittest

from audiotools import Song


# Use the existing media file in the same directory as the unittest for
# testing purposes. The test file is freely available from
# http://www.machinaesupremacy.com/downloads/
testfile = os.path.join(os.path.dirname(__import__(__name__).__file__),
                        'test.flac')


class TestSongMetadata(unittest.TestCase):
    """
    Unittest getting and setting the metadata of a Song object.

    """
    def test_get_existing(self):
        """
        Test reading lower case metadata from a Song object.

        """
        song = Song(testfile)
        self.assertEqual(song['artist'], 'Machinae Supremacy')
        self.assertEqual(song['album'], "Jets'n'Guns")
        self.assertEqual(song['track'], '4')

    def test_get_nonexisting(self):
        """
        Test that a KeyError is thrown when trying to read non-existing
        metadata.

        """
        song = Song(testfile)
        with self.assertRaises(KeyError) as ctx:
            song['I do not exist']
        self.assertEqual(ctx.exception.args[0], 'Metadata not found')

    def test_get_case_insensitive(self):
        """
        Test that keys for getting Song metadata are case insensitive.

        """
        song = Song(testfile)
        self.assertEqual(song['artist'], song['ARTIST'])
        self.assertEqual(song['album'], song['AlBum'])

    def test_set_stringvalue(self):
        """
        Test setting a stringvalue as new metadata.

        """
        song = Song(testfile)
        song['artist'] = 'MS'
        self.assertEqual(song['artist'], 'MS')
        song['artist'] = 'MaSu'
        self.assertEqual(song['artist'], 'MaSu')

    def test_set_nonstringvalue(self):
        """
        Test that the metadata value is converted to a string when it
        is set.

        """
        song = Song(testfile)
        song['track'] = 5
        self.assertEqual(song['track'], '5')
        song['track'] = 4
        self.assertEqual(song['track'], '4')

    def test_set_case_insensitive(self):
        """
        Test that the key for setting a string is case insensitive.

        """
        song = Song(testfile)
        song['ALBUM'] = 'JnG'
        self.assertEqual(song['album'], 'JnG')

    def test_delete(self):
        """
        Test deleting a subscript actually deletes the key.

        """
        song = Song(testfile)
        self.assertIsNotNone(song['album'])
        del song['album']
        with self.assertRaises(KeyError):
            song['I do not exist']


if __name__ == '__main__':
    unittest.main()
