'''Drive the real USI command loop against a supplied CPU probe model.'''

import queue
import subprocess
import sys
from pathlib import Path
from threading import Thread
from typing import TextIO

from deepshogi.board import Board
from deepshogi.usi import usi_string_to_move


def collect(stream: TextIO, output: queue.Queue[str]) -> None:
    '''Copy stream (TextIO) lines into output (Queue[str]); return None.'''
    # Drain stdout continuously so protocol waits can have explicit timeouts.
    for line in stream:
        output.put(line.strip())


def main(path: str) -> None:
    '''Verify USI startup and searches using path (str); return None.'''
    command = [
        sys.executable, str(Path(__file__).parents[1] / 'run.py'), path,
        '--gpus=-1', '--threads', '2', '--threads-per-gpu', '1',
        '--visits', '32', '--max-visits', '128', '--timelimit', '1',
        '--initial-turn', '0', '--check-search-depth', '3', '--check-search-node', '100',
    ]
    with subprocess.Popen(
            command, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, text=True, bufsize=1) as process:
        assert process.stdin is not None and process.stdout is not None
        output: queue.Queue[str] = queue.Queue()
        reader = Thread(target=collect, args=(process.stdout, output), daemon=True)
        reader.start()
        try:
            # Wait for each protocol response before sending the next command.
            for request, expected in [
                ('usi', 'usiok'), ('isready', 'readyok'),
                ('usinewgame\nposition startpos\ngo byoyomi 1000', 'bestmove '),
                ('position startpos moves 7g7f\ngo byoyomi 1000', 'bestmove '),
            ]:
                process.stdin.write(request + '\n')
                process.stdin.flush()
                while True:
                    line = output.get(timeout=15)
                    if line.startswith(expected):
                        if expected == 'bestmove ':
                            board = Board()
                            if '7g7f' in request:
                                board.play(*usi_string_to_move('7g7f'))
                            assert usi_string_to_move(line.split()[1]) in board.get_legal_moves()
                        break
            process.stdin.write('quit\n')
            process.stdin.flush()
            assert process.wait(timeout=10) == 0
        finally:
            if process.poll() is None:
                process.kill()
                process.wait()
            reader.join(timeout=5)


if __name__ == '__main__':
    main(sys.argv[1])
