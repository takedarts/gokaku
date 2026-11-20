import errno
import os
import shutil
import tempfile
from pathlib import Path


def move_file(source: str | Path, destination: str | Path, *, overwrite: bool) -> None:
    '''Move a file created in the OS temporary directory to its destination.

    On the same file system, move it at once by renaming.
    On another file system, copy it to a hidden file in the destination directory and then
    replace the destination, so that a partially written file never appears under the
    destination name and an existing destination is never corrupted.

    Args:
        source (str | Path): Path of the file to move.
        destination (str | Path): Path of the destination file.
        overwrite (bool): True to replace an existing destination.
    Raises:
        FileExistsError: If overwrite is False and the destination already exists.
    '''
    source_path = Path(source)
    destination_path = Path(destination)

    # Detect an existing destination before moving when overwriting is not allowed.
    if not overwrite and (destination_path.exists() or destination_path.is_symlink()):
        raise FileExistsError(destination_path)

    # On the same file system, move to the destination at once by renaming.
    try:
        os.replace(source_path, destination_path)
        return
    except OSError as error:
        if error.errno != errno.EXDEV:
            raise

    # On another file system, copy to a hidden file in the destination directory.
    descriptor, staging = tempfile.mkstemp(
        prefix=f'.{destination_path.name}.', suffix='.tmp', dir=destination_path.parent)
    os.close(descriptor)
    staging_path = Path(staging)

    try:
        # Copy including permissions, and replace the destination after the copy completes.
        shutil.copy2(source_path, staging_path)

        if not overwrite and (destination_path.exists() or destination_path.is_symlink()):
            raise FileExistsError(destination_path)

        os.replace(staging_path, destination_path)
    except BaseException:
        # On failure, do not leave the hidden file in the destination directory.
        staging_path.unlink(missing_ok=True)
        raise

    # Remove the source file after replacing the destination.
    source_path.unlink()
