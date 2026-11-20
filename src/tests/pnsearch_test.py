'''DF-PN regressions adapted from DeepShogi's PN search tests.'''

import tempfile
import unittest
from pathlib import Path

from deepshogi.board import Board
from deepshogi.pnsearch import PnSearch
from tests.native import run_native_test

# Positions extracted from upstream records 001 through 004; the suffix of each key indicates
# how many recorded moves remained before the end of the game.
POSITIONS = {
    '001_1': (
        '9/kSn1g4/2B2s2+P/l1p6/gp1NPPP2/1PPn3P1/+p1N2R2L/p2K5/L8 '
        'b RBSL7P2gs 275'),
    '001_3': (
        '9/2n1g4/k1B2s2+P/l1p6/gp1NPPP2/1PPn3P1/+p1N2R2L/p2K5/L8 '
        'b RB2SL7P2gs 273'),
    '002_31': (
        'l6+Rl/1p2k1n2/r2ppb3/pPps1g2p/Pn5N1/1GPS4P/3PP+l+p2/bK1G1+p3/+n1+s5L '
        'w GSP3p 112'),
    '003_1': (
        'KB6+R/GS1L3SL/1+SBS2p2/5p3/3+R4p/1P4+n2/3+nP1lpP/1g5k1/9 '
        'w 2G2N8Pl3p 254'),
    '004_1': (
        '+N+L1R5/2+N2+P3/9/L1+P3p2/K1P3g2/PP+R6/1G4+s+p+l/4g+n+bbk/1S3+p1ss '
        'b G3Pnl7p 237'),
}
MATE_SFEN = '4k4/9/4K4/9/9/9/9/9/9 b G 1'


class NativePnSearchTest(unittest.TestCase):
    '''Run the C++ PN search regressions and benchmark.'''

    def test_dfpn(self) -> None:
        '''Check DF-PN thresholds, progress correction, and loop cloning.'''
        run_native_test('dfpn_native_test')

    def test_pnsearch(self) -> None:
        '''Check node management and loop detection of PN search.'''
        run_native_test('pnsearch_native_test')

    def test_pnsearch_abort(self) -> None:
        '''Check aborting PN search, restoring the board, and reusing engines.'''
        run_native_test('pnsearch_abort_native_test')

    def test_pnsearch_cache(self) -> None:
        '''Check collisions and generation updates of the PN search cache.'''
        run_native_test('pnsearch_cache_native_test')

    def test_pnsearch_moves(self) -> None:
        '''Check legal move storage and node cloning in PN search.'''
        run_native_test('pnsearch_moves_native_test')

    def test_dfpn_benchmark(self) -> None:
        '''Solve a one-move mate with the benchmark and check the solved and verified flags.'''
        # Input only one known one-move mate position.
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'positions.sfen'
            path.write_text(f'{MATE_SFEN}\n', encoding='utf-8')
            output = run_native_test('dfpn_benchmark', str(path), '1', '1', '1000', '1')

        # Check the position number, solved flag, verified flag, and returned move count in TSV.
        rows = output.splitlines()
        self.assertEqual(len(rows), 1, output)
        fields = rows[0].split('\t')
        self.assertEqual(len(fields), 7, output)
        self.assertEqual(fields[:3], ['1', '1', '1'], output)
        self.assertEqual(fields[6], '1', output)


class PnSearchTest(unittest.TestCase):
    '''Check depth handling, capacity limits, and preservation of the input board.'''

    def check_line(self, sfen: str, moves: list) -> None:
        '''Validate a mating line from sfen (str) and moves (list); return None.'''
        # Replay every move and require the final defender to have no legal reply.
        replay = Board(sfen)
        for index, move in enumerate(moves):
            self.assertIn(move, replay.get_legal_moves(check_only=index % 2 == 0))
            replay.play(*move)
        self.assertEqual(replay.get_legal_moves(), [])

    def test_input_unchanged(self) -> None:
        '''Check successful, unsolved, and exhausted searches; return None.'''
        # Reuse each engine to expose cache-generation and board-restoration errors.
        for nodes in (0, 1, 50000):
            search = PnSearch(nodes=nodes)
            for name in ('001_3', '002_31', '003_1'):
                board = Board(POSITIONS[name])
                before = board.get_sfen()
                legal_moves = board.get_legal_moves()
                with self.subTest(nodes=nodes, name=name):
                    first = search.get_checkmate_moves(board, depth=31)
                    self.assertEqual(board.get_sfen(), before)
                    self.assertEqual(board.get_legal_moves(), legal_moves)
                    self.assertEqual(search.get_checkmate_moves(board, depth=31), first)
                    self.assertEqual(board.get_sfen(), before)
                    if nodes <= 1:
                        self.assertEqual(first, [])

    def test_depth_limit(self) -> None:
        '''Check one- and three-ply boundaries and even-depth rounding; return None.'''
        board = Board(POSITIONS['001_3'])
        search = PnSearch(nodes=50000)
        self.assertEqual(search.get_checkmate_moves(board, depth=1), [])
        moves = search.get_checkmate_moves(board, depth=3)
        self.assertEqual(len(moves), 3)
        self.assertEqual(search.get_checkmate_moves(board, depth=2), moves)
        self.check_line(board.get_sfen(), moves)

    def test_one_move_depth(self) -> None:
        '''Check zero and negative depth normalization on a one-move mate; return None.'''
        board = Board(MATE_SFEN)
        before = board.get_sfen()
        search = PnSearch(nodes=1000)
        for depth in (1, 0, -1, -2):
            with self.subTest(depth=depth):
                moves = search.get_checkmate_moves(board, depth=depth)
                self.assertEqual(len(moves), 1)
                self.assertEqual(board.get_sfen(), before)
                self.check_line(before, moves)

    def test_depth_extension(self) -> None:
        '''Check shared-node depth correction beyond the initial depth; return None.'''
        # This upstream regression used to stall when the requested depth decreased.
        board = Board(
            'l6+Rl/1p2k1n2/r2ppb3/pPps1g2p/Pn5N1/1GPS4P/3PP+l+p2/'
            'bK1G1+p3/+n1+s5L w GSP3p 112')
        before = board.get_sfen()
        moves = PnSearch(nodes=100000).get_checkmate_moves(board, depth=29)
        self.assertGreater(len(moves), 29)
        self.assertEqual(board.get_sfen(), before)
        self.check_line(before, moves)

    def test_draw_turn(self) -> None:
        '''Check that the game draw limit does not truncate mate search; return None.'''
        search = PnSearch(nodes=100000)
        for remaining in (1, 3):
            original = Board(POSITIONS[f'001_{remaining}'])
            expected = search.get_checkmate_moves(original, depth=3)
            self.assertEqual(len(expected), remaining)
            turn = original.get_turn()
            for draw_turn in (turn - 1, turn, turn + 1, turn + 10):
                with self.subTest(remaining=remaining, draw_turn=draw_turn):
                    board = Board(original.get_sfen(), draw_turn=draw_turn)
                    self.assertEqual(search.get_checkmate_moves(board, depth=3), expected)
                    self.assertEqual(board.get_sfen(), original.get_sfen())
                    self.check_line(board.get_sfen(), expected)

    def test_search(self) -> None:
        '''Check longer mates and non-mating positions from upstream games; return None.'''
        search = PnSearch(nodes=100000)
        for name, solved in [('001_3', True), ('002_31', True),
                             ('003_1', False), ('004_1', False)]:
            with self.subTest(name=name):
                board = Board(POSITIONS[name])
                moves = search.get_checkmate_moves(board, depth=31)
                self.assertEqual(bool(moves), solved)
                if solved:
                    self.check_line(board.get_sfen(), moves)
