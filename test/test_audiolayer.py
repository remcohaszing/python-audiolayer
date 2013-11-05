import functools
import os
import shutil
import unittest

from audiolayer import NoMediaException
from audiolayer import Song


# Use the existing media file in the same directory as the unittest for
# testing purposes. The test file is freely available from
# http://www.machinaesupremacy.com/downloads/
testfile = os.path.join(os.path.dirname(__import__(__name__).__file__),
                        'test.flac')


def cleanup(filename):
    """
    This function should be used as a decorator. The filename is
    appended to the function args. After finishing the function the
    file by that filename is deleted.

    """
    def wrapper(fn):
        @functools.wraps(fn)
        def inner(*args, **kwargs):
            args += (filename,)
            try:
                result = fn(*args, **kwargs)
            except:
                if os.path.isfile(filename):
                    os.remove(filename)
                raise
            if os.path.isfile(filename):
                os.remove(filename)
            return result
        return inner
    return wrapper


class TestInitSong(unittest.TestCase):
    """
    Test that it is possible to initialize a Song object.

    """
    def test_valid_song(self):
        """
        Test initializing a Song correctly.

        """
        song = Song(testfile)
        self.assertIsInstance(song, Song)

    def test_no_args(self):
        """
        Test initializing a song without a filename.

        """
        with self.assertRaises(TypeError):
            Song()

    def test_invalid_filetype(self):
        """
        Test initializing a Song using a file of an invalid type.

        """
        errfile = __import__(__name__).__file__
        with self.assertRaises(NoMediaException) as ctx:
            Song(errfile)
        # Should use filename instead of args[2] as soon as
        # NoMediaException subclasses OSError.
        self.assertEqual(ctx.exception.args[2], errfile)

    def test_directory(self):
        """
        Test initializing using a filename which refers to a directory.

        """
        testdir = os.path.dirname(__import__(__name__).__file__)
        with self.assertRaises(IsADirectoryError) as ctx:
            Song(testdir)
        self.assertEqual(ctx.exception.filename, testdir)

    def test_non_existing_file(self):
        """
        Test initializing a Song using a filename which does not exist.

        """
        errfile = 'f' * 200
        with self.assertRaises(FileNotFoundError) as ctx:
            # File may not exist for this test wo work.
            Song(errfile)
        self.assertEqual(ctx.exception.filename, errfile)

    def test_duplicate_init_call(self):
        """
        Test calling __init__ of an already initialized Song object
        raises a UserWarning.

        """
        song = Song(testfile)
        with self.assertRaises(UserWarning):
            song.__init__('f' * 200)
        with self.assertRaises(UserWarning):
            song.__init__(os.path.dirname(__import__(__name__).__file__))
        with self.assertRaises(UserWarning):
            song.__init__(testfile)


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

    def test_special_character(self):
        """
        Test setting and getting special characters.

        """
        song = Song(testfile)
        self.assertEqual(song['title'], 'Megascorcher')
        song['title'] = 'Mégäscørcher'
        self.assertEqual(song['title'], 'Mégäscørcher')

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

    def test_contains(self):
        """
        Test values 'in' a Song.

        """
        song = Song(testfile)
        self.assertTrue('artist' in song)
        self.assertTrue('artiSt' in song)
        self.assertTrue('TitLe' in song)
        self.assertFalse('something non existing' in song)

    def test_forloop(self):
        """
        Test looping through a song using a for loop is handled
        correctly.

        """
        song = Song(testfile)
        tags = [tag for tag in song]
        self.assertSequenceEqual(tags, [
            'track', 'title', 'artist', 'album', 'album artist',
            'album_artist', 'disc', 'date', 'genre'])

    def test_len(self):
        """
        Test that the length function of a song object is properly
        implemented.

        """
        song = Song(testfile)
        self.assertEqual(len(song), 9)

    def test_str(self):
        """
        Test the implementation of str(song)

        """
        song = Song(testfile)
        value = str(song)
        self.assertTrue(value.startswith('audiolayer.Song('))
        self.assertTrue(value.endswith(')'))
        for tag in song:
            self.assertIn(tag, value)
            self.assertIn(song[tag], value)


class TestSaveSong(unittest.TestCase):
    """
    Test functionality for saving the song metadata.

    """
    @cleanup('out_unchanged.flac')
    def test_with_filename(self, filename):
        """
        Test saving the file to a different location.

        """
        song = Song(testfile)
        song.save(filename=filename)
        copy = Song(filename)
        for tag in song:
            self.assertIn(tag, copy)
            self.assertEqual(song[tag], copy[tag])
        os.remove('out_unchanged.flac')

    @cleanup('out_changed_meta.flac')
    def test_with_filename_and_changed_metadata(self, filename):
        """
        Test saving the file to a different location when different
        metadata has been set.

        """
        song = Song(testfile)
        song['artist'] = 'MaSu'
        song.save(filename=filename)
        copy = Song(filename)
        self.assertEqual(copy['artist'], 'MaSu')

    @cleanup('out_name_unspecified.flac')
    def test_no_filename(self, filename):
        """
        Test saving metadata to the original input file.

        """
        shutil.copy(testfile, filename)
        song = Song(filename)
        song['track'] = 5
        song.save()
        new = Song(filename)
        self.assertEqual(new['track'], '5')

    def test_nonexisting_directory(self):
        """
        Test saving the file to a non existing directory.

        """
        song = Song(testfile)
        with self.assertRaises(FileNotFoundError):
            song.save('non/existing/directory/out.flac')


class TestSongAudioInfo(unittest.TestCase):
    """
    Test getting some data about the audio stream.

    """
    def test_duration(self):
        """
        Test the duration of a file is retrieved correctly in seconds.

        """
        song = Song(testfile)
        self.assertAlmostEqual(song.duration, 119.188, 3)
        self.assertIs(song.duration, song.duration)

    def test_sample_rate(self):
        """
        Test the sample rate of the file is retrieved correctly.

        """
        song = Song(testfile)
        self.assertEqual(song.sample_rate, 44100)
        self.assertIs(song.sample_rate, song.sample_rate)

    def test_channels(self):
        """
        Test the amount of channels of the audio stream is retrieved
        correctly.

        """
        song = Song(testfile)
        self.assertEqual(song.channels, 2)
        self.assertIs(song.channels, song.channels)


class TestSongPlayback(unittest.TestCase):
    """
    Test the playback is handled correctly and the play method does not
    raise any exceptions

    """
    @unittest.skip('This test takes too long to finish,'
                   'because the entire song is played.')
    def test_playback(self):
        song = Song(testfile)
        self.assertIsNone(song.play())


if __name__ == '__main__':
    unittest.main()
