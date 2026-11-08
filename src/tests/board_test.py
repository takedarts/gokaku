import unittest

import numpy as np

from deepshogi.board import Board
from deepshogi.config import COLOR_BLACK, COLOR_WHITE, DEFAULT_INITIAL_SFEN


class BoardTest(unittest.TestCase):
    '''Check native board reinitialization with changing draw limits.'''

    def test_native_reinitialize(self) -> None:
        '''Preserve entering-king scores while resetting the position and draw limit.
        Returns:
            None: No return value.
        '''
        # Repeatedly initialize one board with nondefault entering-king scores.
        scores = (31, 24)
        board = Board(nyugyoku_scores=scores)
        for draw_turn in (256, 300, 512, 256):
            with self.subTest(draw_turn=draw_turn):
                fields = DEFAULT_INITIAL_SFEN.split()
                fields[3] = str(draw_turn - 10)
                sfen = ' '.join(fields)
                board.native.initialize(sfen, draw_turn)
                expected = Board(sfen, nyugyoku_scores=scores, draw_turn=draw_turn)
                self.assertEqual(board.get_sfen(), sfen)
                self.assertEqual(board.get_legal_moves(), expected.get_legal_moves())
                for color in (COLOR_BLACK, COLOR_WHITE):
                    np.testing.assert_array_equal(
                        board.get_inputs(color), expected.get_inputs(color))

                # Reset state changed by moves on the next initialization.
                move = board.get_legal_moves()[0]
                board.play(*move)
                expected.play(*move)
                np.testing.assert_array_equal(board.get_inputs(), expected.get_inputs())

