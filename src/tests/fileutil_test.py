import errno
import os
import tempfile
import unittest
from pathlib import Path
from typing import Any
from unittest.mock import patch

from deepshogi.fileutil import move_file

# Function before replacing the move by renaming.
REPLACE = os.replace


def replace_across_devices(source: Any, destination: Any) -> None:
    '''Fail only moves to another file system.

    Args:
        source (Any): Source path.
        destination (Any): Destination path.
    Raises:
        OSError: If the source and destination directories differ.
    '''
    # Actually replace within the same directory, and treat others as different devices.
    if Path(source).parent != Path(destination).parent:
        raise OSError(errno.EXDEV, os.strerror(errno.EXDEV))

    REPLACE(source, destination)


class MoveFileTest(unittest.TestCase):
    '''Check moving a temporary file to its destination.'''

    def setUp(self) -> None:
        '''Prepare the source and destination directories.'''
        self.temp = tempfile.TemporaryDirectory()
        self.source_dir = Path(self.temp.name) / 'source'
        self.destination_dir = Path(self.temp.name) / 'destination'
        self.source_dir.mkdir()
        self.destination_dir.mkdir()
        self.source = self.source_dir / 'model.ep'
        self.destination = self.destination_dir / 'best.ep'
        self.source.write_bytes(b'new')

    def tearDown(self) -> None:
        '''Remove the created directories.'''
        self.temp.cleanup()

    def test_moves_on_same_filesystem(self) -> None:
        '''Move the source to the destination on the same file system.'''
        move_file(self.source, self.destination, overwrite=False)

        self.assertEqual(self.destination.read_bytes(), b'new')
        self.assertFalse(self.source.exists())

    def test_rejects_existing_destination(self) -> None:
        '''Do not change an existing destination when overwriting is not allowed.'''
        self.destination.write_bytes(b'old')

        with self.assertRaises(FileExistsError):
            move_file(self.source, self.destination, overwrite=False)

        self.assertEqual(self.destination.read_bytes(), b'old')
        self.assertTrue(self.source.exists())

    def test_overwrites_across_filesystems(self) -> None:
        '''Copy and replace on another file system without leaving a hidden file.'''
        self.destination.write_bytes(b'old')

        with patch('deepshogi.fileutil.os.replace', side_effect=replace_across_devices):
            move_file(self.source, self.destination, overwrite=True)

        self.assertEqual(self.destination.read_bytes(), b'new')
        self.assertFalse(self.source.exists())
        self.assertEqual(list(self.destination_dir.iterdir()), [self.destination])

    def test_failed_copy_keeps_destination(self) -> None:
        '''Keep the destination and the source and remove the hidden file when copying fails.'''
        self.destination.write_bytes(b'old')

        with patch('deepshogi.fileutil.os.replace', side_effect=replace_across_devices), \
                patch('deepshogi.fileutil.shutil.copy2', side_effect=OSError('disk full')):
            with self.assertRaises(OSError):
                move_file(self.source, self.destination, overwrite=True)

        self.assertEqual(self.destination.read_bytes(), b'old')
        self.assertTrue(self.source.exists())
        self.assertEqual(list(self.destination_dir.iterdir()), [self.destination])


if __name__ == '__main__':
    unittest.main()
