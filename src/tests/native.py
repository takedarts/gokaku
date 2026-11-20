'''Common helpers to build and run the C++ tests.'''

import os
import subprocess
from functools import cache
from pathlib import Path
from tempfile import TemporaryDirectory

import cmake


SOURCE_DIR = Path(__file__).resolve().with_name('native')
BUILD_CONFIG = 'Release'
EXECUTABLE_SUFFIX = '.exe' if os.name == 'nt' else ''


@cache
def _build_root() -> TemporaryDirectory[str]:
    '''Keep the build directory until the test process ends.

    Returns:
        TemporaryDirectory[str]: Build directory dedicated to the test process.
    '''
    # Reuse objects within the same run and separate them from concurrent test runs.
    return TemporaryDirectory(prefix='gokaku-native-tests-')


def _run_command(
    command: list[str], timeout: float, env: dict[str, str] | None = None,
) -> str:
    '''Run a build or test command and report its output on failure.

    Args:
        command (list[str]): Executable and arguments.
        timeout (float): Maximum number of seconds to wait for the command.
        env (dict[str, str] | None): Environment of the child process. None inherits the parent.

    Returns:
        str: Standard output when the command succeeds.
    '''
    # Report launch failures, abnormal exits, and timeouts to unittest as failures.
    try:
        result = subprocess.run(
            command, check=True, capture_output=True, text=True, errors='replace',
            timeout=timeout, env=env)
    except subprocess.CalledProcessError as error:
        raise AssertionError(f'{error}\n{error.stdout}{error.stderr}') from error
    except subprocess.TimeoutExpired as error:
        raise AssertionError(f'{error}\n{error.stdout!r}\n{error.stderr!r}') from error
    except OSError as error:
        raise AssertionError(f'{command!r}: {error}') from error
    return result.stdout


@cache
def _configure_build(use_torch: bool) -> tuple[str, Path, dict[str, str] | None]:
    '''Configure the CMake project dedicated to the tests.

    Args:
        use_torch (bool): Whether to load LibTorch for the Player test.

    Returns:
        tuple[str, Path, dict[str, str] | None]: CMake, build directory, and runtime environment.
    '''
    # Use the CMake package in the running Python environment.
    cmake_name = f'cmake{EXECUTABLE_SUFFIX}'
    cmake_path = str(Path(cmake.CMAKE_BIN_DIR) / cmake_name) if hasattr(
        cmake, 'CMAKE_BIN_DIR') else cmake_name
    build_dir = Path(_build_root().name) / ('player' if use_torch else 'pnsearch')
    options = [f'-DCMAKE_BUILD_TYPE={BUILD_CONFIG}']
    env: dict[str, str] | None = None

    # Use LibTorch from the current PyTorch environment only for the Player test.
    if use_torch:
        import torch

        assert torch.__file__ is not None
        torch_dir = Path(torch.__file__).resolve().parent
        torch_cmake_dir = torch_dir / 'share' / 'cmake' / 'Torch'
        options.extend([
            '-DDEEPSHOGI_TEST_PLAYER=ON', f'-DTorch_DIR={torch_cmake_dir}',
        ])
        if os.name == 'nt':
            dll_path = os.pathsep.join([str(torch_dir / 'lib'), os.environ.get('PATH', '')])
            env = {**os.environ, 'PATH': dll_path}

    # Configure the test targets referring to the generated production Config.h.
    _run_command([cmake_path, '-S', str(SOURCE_DIR), '-B', str(build_dir), *options], 120)
    return cmake_path, build_dir, env


def run_native_test(target: str, *args: str) -> str:
    '''Build the specified C++ target incrementally and run it.

    Args:
        target (str): CMake target name.
        *args (str): Arguments passed to the C++ executable.

    Returns:
        str: Standard output of the C++ executable.
    '''
    # Build only the target and launch the completed executable in a separate process.
    cmake_path, build_dir, env = _configure_build(target == 'player_wait_native_test')
    _run_command([
        cmake_path, '--build', str(build_dir), '--config', BUILD_CONFIG, '--target', target,
    ], 600)
    executable = build_dir / 'bin' / BUILD_CONFIG / f'{target}{EXECUTABLE_SUFFIX}'
    return _run_command([str(executable), *args], 120, env)
