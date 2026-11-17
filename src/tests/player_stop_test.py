import tempfile
import unittest
from pathlib import Path
from queue import Queue
from threading import Condition, Event, Thread
from typing import Any, ClassVar
from unittest.mock import Mock, patch

import numpy as np
import torch

from deepshogi.board import Board
from deepshogi.config import (
    COLOR_BLACK, COLOR_NONE, COLOR_WHITE, DEFAULT_DRAW_TURN, DEFAULT_INITIAL_SFEN,
    MODEL_INPUT_SIZE, MODEL_VALUE_OFFSET, RESULT_MAX_MOVES, RESULT_NONE, RESULT_SENNICHITE,
)
from deepshogi.player import Candidate, Player, Referee
from deepshogi.processor import Processor
from deepshogi.usi import USIEngine, usi_string_to_move


class StopModel(torch.nn.Module):
    '''Provide deterministic CPU search outputs without a trained model.'''

    def forward(self, inputs: torch.Tensor) -> torch.Tensor:
        '''Map input features (Tensor) to policy and game value outputs (Tensor).
        Args:
            inputs (torch.Tensor): Batched model input features.
        Returns:
            torch.Tensor: Batched policy, game value, and auxiliary outputs.
        '''
        # Match the native output layout using a single input feature.
        value = torch.sigmoid(inputs[:, :1])
        policy = value.expand(-1, MODEL_VALUE_OFFSET)
        return torch.cat((policy, value, value * 0, value * 0 + 1, inputs[:, :6]), dim=1)


class PlayerStopTest(unittest.TestCase):
    '''Verify Python stop notifications and suppression of subsequent searches.'''

    def setUp(self) -> None:
        '''Prepare a player with mocked native search.
        '''
        self.player = object.__new__(Player)
        self.player.native = Mock()
        self.player.processor = Mock()
        self.player.referee = Mock()
        self.stop_event = Event()
        self.player.native.get_candidates.return_value = [
            ((6, 6), (6, 5), False, COLOR_BLACK, 100, 0.5, 0.0, 1.0, []),
            ((1, 6), (1, 5), False, COLOR_BLACK, 100, 0.5, 0.0, 1.0, []),
        ]

    def test_stop_before_start(self) -> None:
        '''Do not start or extend a search when stopped before startup.
        '''
        self.stop_event.set()
        self.assertEqual(self.player.evaluate(100, stop_event=self.stop_event), [])
        self.player.native.start_evaluation.assert_not_called()
        self.player.native.wait_evaluation.assert_not_called()

    def test_stop_during_start(self) -> None:
        '''Notify stopping after startup when it races with search startup.
        '''
        # Simulate native startup clearing the stopped state.
        self.player.native.start_evaluation.side_effect = lambda *args: self.stop_event.set()
        result = self.player.evaluate(100, extends=3, stop_event=self.stop_event)
        self.assertEqual(len(result), 2)
        self.player.native.stop_evaluation.assert_called_once()
        self.assertEqual(self.player.native.start_evaluation.call_count, 1)

    def test_stop_prevents_extensions(self) -> None:
        '''Return available candidates without extensions when stopped during a result wait.
        '''
        self.player.native.wait_evaluation.side_effect = lambda *args: self.stop_event.set()
        result = self.player.evaluate(100, extends=3, stop_event=self.stop_event)
        self.assertEqual(len(result), 2)
        self.assertEqual(self.player.native.start_evaluation.call_count, 1)
        self.assertEqual(self.player.native.wait_evaluation.call_count, 1)

    def test_random_stop_before_first_evaluation(self) -> None:
        '''Return a legal random fallback when stopped before the first evaluation.
        '''
        self.stop_event.set()
        self.player.native.get_candidates.return_value = []
        board = Board()
        with patch.object(Player, 'get_board', return_value=board):
            candidate = self.player.get_random_candidate(stop_event=self.stop_event)
        self.assertIn(
            (candidate.src, candidate.dst, candidate.promote), board.get_legal_moves())
        self.player.native.start_evaluation.assert_not_called()


class NativePlayerStopTest(unittest.TestCase):
    '''Verify actual C++ wait interruption and search restart after stopping.'''

    processor: ClassVar[Processor]

    @classmethod
    def setUpClass(cls) -> None:
        '''Prepare a small CPU model and actual inference processor.
        '''
        model = StopModel()
        model.eval()
        # Keep the model file until class cleanup because native loading is asynchronous.
        directory = tempfile.TemporaryDirectory()
        cls.addClassCleanup(directory.cleanup)
        path = Path(directory.name) / 'stop_test.pt'
        torch.jit.trace(model, torch.zeros(1, MODEL_INPUT_SIZE)).save(str(path))
        cls.processor = Processor(
            path, gpus=[-1], deterministic=True, batch_size=1, threads_per_gpu=1)
        cls.addClassCleanup(delattr, cls, 'processor')

    def _assert_wait_is_interrupted(self, player: Player) -> None:
        '''Verify player (Player) waits are interrupted by stopping; return None.'''
        started = Event()
        finished = Event()

        def wait() -> None:
            '''Wait for C++ search results and signal completion; return None.'''
            started.set()
            player.native.wait_evaluation(1_000_000_000, 2.0, True)
            finished.set()

        worker = Thread(target=wait, daemon=True)
        worker.start()
        try:
            self.assertTrue(started.wait(1.0))
            self.assertFalse(finished.wait(0.02))
            player.stop_evaluation()
            self.assertTrue(finished.wait(1.0))
        finally:
            # Release the first-visit wait and join the thread even if the test fails.
            if not finished.is_set():
                player.native.start_evaluation(False, 0, 1.0, 0.0)
            worker.join(3.0)
            player.stop_evaluation()
        self.assertFalse(worker.is_alive())

    def test_failed_play_does_not_cause_sennichite(self) -> None:
        '''Check that three failed moves from the initial position do not cause false repetition.
        '''
        player = Player(self.processor, check_node_depth=0)
        initial_sfen = player.get_board().get_sfen()

        # Reject moves from empty squares in real C++ code and verify exception propagation.
        for _ in range(3):
            with self.assertRaisesRegex(ValueError, 'Position does not have a piece'):
                player.play((0, 4), (0, 3))

        # Do not count failed moves in position occurrences or consecutive checks.
        board = player.get_board()
        self.assertEqual(board.get_sfen(), initial_sfen)
        self.assertEqual(player.referee.judge(board), (False, COLOR_NONE, RESULT_NONE))
        self.assertEqual(player.referee.board_repeats, {})
        self.assertEqual(player.referee.check_counts, [0, 0])

    def test_failed_play_preserves_referee_state(self) -> None:
        '''Check referee state survives failed moves and later legal moves update history.
        '''
        for attacking_color in (COLOR_BLACK, COLOR_WHITE):
            for checking in (False, True):
                with self.subTest(attacking_color=attacking_color, checking=checking):
                    # Create history and check counts by checking the king and letting it escape.
                    black = attacking_color == COLOR_BLACK
                    sfen = ('4k4/9/3R5/9/9/9/9/9/K8 b - 1' if black else
                            'k8/9/9/9/9/9/3r5/9/4K4 w - 1')
                    player = Player(self.processor, initial_sfen=sfen, check_node_depth=0)
                    rook_rank = 2 if black else 6
                    king_rank = 0 if black else 8
                    player.play((5, rook_rank), (4, rook_rank))
                    player.play((4, king_rank), (5, king_rank))
                    if checking:
                        player.play((4, rook_rank), (5, rook_rank))
                    else:
                        player.play((8, 8 - king_rank), (7, 8 - king_rank))
                    board = player.get_board()
                    self.assertEqual(board.is_check(), checking)
                    repeats = dict(player.referee.board_repeats)
                    check_counts = player.referee.check_counts.copy()
                    self.assertEqual(check_counts, [1, 0] if black else [0, 1])

                    # Prevent failed moves from incrementing or resetting checks or changing
                    # position history.
                    with self.assertRaisesRegex(ValueError, 'Position does not have a piece'):
                        player.play((0, 4), (0, 3))
                    self.assertEqual(player.get_board().get_sfen(), board.get_sfen())
                    self.assertEqual(player.referee.board_repeats, repeats)
                    self.assertEqual(player.referee.check_counts, check_counts)

                    # Record the previous position exactly once after the next legal move.
                    expected_referee = Referee()
                    expected_referee.board_repeats = repeats
                    expected_referee.check_counts = check_counts
                    expected_referee.update(board)
                    move = board.get_legal_moves()[0]
                    self.assertEqual(player.play(*move), move)
                    board.play(*move)
                    self.assertEqual(player.get_board().get_sfen(), board.get_sfen())
                    self.assertEqual(player.referee.board_repeats,
                                     expected_referee.board_repeats)
                    self.assertEqual(player.referee.check_counts, expected_referee.check_counts)

    def test_failed_play_preserves_search_and_history(self) -> None:
        '''Check failed moves preserve board and history during active and stopped searches.
        '''
        for searching in (False, True):
            with self.subTest(searching=searching):
                # Create position history and consecutive checks, then run native search.
                player = Player(
                    self.processor, initial_sfen='4k4/9/3R5/9/9/9/9/9/K8 b - 1',
                    threads=2, max_visits=1_000_000, check_node_depth=0)
                for src, dst in (((5, 2), (4, 2)), ((4, 0), (5, 0)), ((4, 2), (5, 2))):
                    player.play(src, dst)
                before = player.get_board()
                repeats = dict(player.referee.board_repeats)
                check_counts = player.referee.check_counts.copy()
                self.assertEqual(check_counts, [1, 0])
                player.native.start_evaluation(True, 16, 0.75, 0.25)
                player.native.wait_evaluation(4, 2.0, not searching)

                try:
                    # Preserve board and referee state across repeated failures and propagate
                    # exceptions.
                    for _ in range(3):
                        with self.assertRaisesRegex(ValueError, 'Position does not have a piece'):
                            player.play((0, 4), (0, 3))
                        after = player.get_board()
                        self.assertEqual(after.get_sfen(), before.get_sfen())
                        np.testing.assert_array_equal(after.get_inputs(), before.get_inputs())
                        self.assertEqual(player.referee.board_repeats, repeats)
                        self.assertEqual(player.referee.check_counts, check_counts)

                    # Continue active search without restarting and keep visits unchanged if
                    # stopped.
                    visits = player.get_visits()
                    player.native.wait_evaluation(visits + 16, 2.0, False)
                    if searching:
                        self.assertGreater(player.get_visits(), visits)
                    else:
                        self.assertEqual(player.get_visits(), visits)
                finally:
                    player.stop_evaluation()

                # Record a legal move once after failure and allow searching from its position.
                expected_referee = Referee()
                expected_referee.board_repeats = repeats.copy()
                expected_referee.check_counts = check_counts.copy()
                expected_referee.update(before)
                move = before.get_legal_moves()[0]
                self.assertEqual(player.play(*move), move)
                before.play(*move)
                self.assertEqual(player.get_board().get_sfen(), before.get_sfen())
                self.assertEqual(player.referee.board_repeats, expected_referee.board_repeats)
                self.assertEqual(player.referee.check_counts, expected_referee.check_counts)
                self.assertTrue(player.evaluate(8, timelimit=2.0))

    def test_successful_play_registers_previous_board(self) -> None:
        '''Check successful moves, including promotion, record only the previous board once.
        '''
        for specify_piece in (False, True):
            with self.subTest(specify_piece=specify_piece):
                # Check explicit promotion and promotion inferred from the resulting piece type.
                player = Player(self.processor, initial_sfen='k8/9/4P4/9/9/9/9/9/4K4 b - 1',
                                check_node_depth=0)
                before = player.get_board()
                src, dst = (4, 2), (4, 1)
                piece = before.get_moved_piece(src, dst, True) if specify_piece else None
                result = player.play(src, dst, promote=not specify_piece, piece=piece)
                self.assertEqual(result, (src, dst, True))

                # Preserve board and return values without recording the resulting position early.
                after = player.get_board()
                expected = Board(before.get_sfen())
                expected.play(src, dst, True)
                self.assertEqual(after.get_sfen(), expected.get_sfen())
                self.assertEqual(player.referee.get_repeats(before), 1)
                self.assertEqual(player.referee.get_repeats(after), 0)
                self.assertEqual(len(player.referee.board_repeats), 1)

    def test_failed_initialize_preserves_game_state(self) -> None:
        '''Check board and referee state survive initialization failure and remain usable.
        '''
        scores = (31, 32)
        player = Player(self.processor, initial_sfen='4k4/9/3R5/9/9/9/9/9/K8 b - 1',
                        nyugyoku_scores=scores, check_node_depth=0, max_visits=64)
        player.initialize(player.get_board().get_sfen(), draw_turn=300)

        # Prepare history, consecutive checks, and a retained search result.
        for src, dst in (((5, 2), (4, 2)), ((4, 0), (5, 0)), ((4, 2), (5, 2))):
            player.play(src, dst)
        self.assertTrue(player.evaluate(8, equally=True, timelimit=1.0))
        before = player.get_board()
        repeats = dict(player.referee.board_repeats)
        check_counts = player.referee.check_counts.copy()
        self.assertTrue(repeats)
        self.assertEqual(check_counts, [1, 0])
        invalid_turn = ' '.join(before.get_sfen().split()[:-1] + ['invalid'])

        for sfen, error_type in (('X8/9/9/9/9/9/9/9/8K b - 1', IndexError),
                                 (invalid_turn, ValueError)):
            with self.subTest(sfen=sfen):
                # Preserve original rules and history if initialization fails after Python stops
                # search.
                with self.assertRaises(error_type):
                    player.initialize(sfen, draw_turn=400)
                after = player.get_board()
                self.assertEqual(after.get_sfen(), before.get_sfen())
                np.testing.assert_array_equal(after.get_inputs(), before.get_inputs())
                self.assertEqual(player.referee.board_repeats, repeats)
                self.assertEqual(player.referee.check_counts, check_counts)
                self.assertEqual(player.referee.draw_turn, 300)

        # Allow play and search after failure and clear referee state on successful initialization.
        move = before.get_legal_moves()[0]
        self.assertEqual(player.play(*move), move)
        before.play(*move)
        self.assertEqual(player.get_board().get_sfen(), before.get_sfen())
        self.assertTrue(player.evaluate(4, timelimit=1.0))
        player.initialize(draw_turn=256)
        expected = Board(nyugyoku_scores=scores, draw_turn=256)
        np.testing.assert_array_equal(player.get_board().get_inputs(), expected.get_inputs())
        self.assertEqual(player.referee.board_repeats, {})
        self.assertEqual(player.referee.check_counts, [0, 0])
        self.assertEqual(player.referee.draw_turn, 256)
        self.assertEqual(player.get_visits(), 0)

    def test_initialize_draw_turn(self) -> None:
        '''Keep referee and native draw limits consistent after reinitialization and moves.
        '''
        # Check default draw limits and entering-king rules immediately after construction.
        scores = (31, 31)
        player = Player(self.processor, nyugyoku_scores=scores, check_node_depth=0)
        expected = Board(nyugyoku_scores=scores)
        np.testing.assert_array_equal(player.get_board().get_inputs(), expected.get_inputs())
        self.assertEqual(player.referee.draw_turn, DEFAULT_DRAW_TURN)

        # Reuse the player and check omitted settings restore defaults rather than previous values.
        for configured_turn in (256, 300, 32767, None, 256):
            draw_turn = DEFAULT_DRAW_TURN if configured_turn is None else configured_turn
            with self.subTest(draw_turn=draw_turn):
                fields = DEFAULT_INITIAL_SFEN.split()
                fields[3] = str(draw_turn - 1)
                sfen = ' '.join(fields)
                if configured_turn is None:
                    player.initialize(sfen)
                else:
                    player.initialize(sfen, draw_turn=configured_turn)
                self.assertEqual(player.referee.draw_turn, draw_turn)
                self.assertFalse(player.referee.board_repeats)
                expected = Board(sfen, nyugyoku_scores=scores, draw_turn=draw_turn)
                np.testing.assert_array_equal(
                    player.get_board().get_inputs(), expected.get_inputs())

                # Check rule inheritance by child nodes and termination at the configured limit.
                for _ in range(2):
                    self.assertFalse(player.referee.judge(player.get_board())[0])
                    move = player.get_board().get_legal_moves()[0]
                    player.play(*move)
                    expected.play(*move)
                    np.testing.assert_array_equal(
                        player.get_board().get_inputs(), expected.get_inputs())
                self.assertEqual(player.get_board().get_turn(), draw_turn)
                self.assertEqual(
                    player.referee.judge(player.get_board()),
                    (True, COLOR_NONE, RESULT_MAX_MOVES))

    def test_draw_turn_in_native_search(self) -> None:
        '''Evaluate children at the draw limit as draws even when reusing the search tree.
        '''
        # Check C++ candidate values directly without Python terminal value corrections.
        player = Player(self.processor, max_visits=64, check_node_depth=0)
        for draw_turn in (1, 2, 1):
            with self.subTest(draw_turn=draw_turn):
                fields = DEFAULT_INITIAL_SFEN.split()
                fields[3] = str(draw_turn)
                player.initialize(' '.join(fields), draw_turn=draw_turn)
                player.native.start_evaluation(True, 0, 1.0, 0.0)
                player.native.wait_evaluation(64, 1.0, True)
                candidates = player.native.get_candidates()
                visited = [candidate for candidate in candidates if candidate[4] > 0]
                self.assertTrue(visited)
                self.assertTrue(all(candidate[6] == 0.0 for candidate in visited))

    def test_sennichite_penalty_matches_native_value(self) -> None:
        '''Check native repetition values match Python terminal corrections for both sides.
        '''
        for penalty in (-0.2, 0.0, 0.2):
            for initial_color in (COLOR_BLACK, COLOR_WHITE):
                with self.subTest(penalty=penalty, initial_color=initial_color):
                    # Move both kings to adjacent files and back so the next move repeats the
                    # initial position.
                    side = 'b' if initial_color == COLOR_BLACK else 'w'
                    player = Player(
                        self.processor, initial_sfen=f'4k4/9/9/9/9/9/9/9/4K4 {side} - 1',
                        max_visits=64, check_node_depth=0, sennichite_penalty=penalty,
                        allowed_repeats=1)
                    first_rank = 8 if initial_color == COLOR_BLACK else 0
                    second_rank = 8 - first_rank
                    for src, dst in (
                            ((4, first_rank), (5, first_rank)),
                            ((4, second_rank), (5, second_rank)),
                            ((5, first_rank), (4, first_rank))):
                        player.play(src, dst)
                    repeated_move = ((5, second_rank), (4, second_rank), False)
                    repeating_color = player.get_color()
                    expected_value = -repeating_color * max(penalty, 0.0)
                    self.assertEqual(player.sennichite_penalty, max(penalty, 0.0))

                    # Get actual C++ search values before Python overrides them.
                    player.native.start_evaluation(True, 0, 1.0, 0.0)
                    player.native.wait_evaluation(64, 2.0, True)
                    native_candidates = {
                        (c.src, c.dst, c.promote): c
                        for c in (Candidate(*data) for data in player.native.get_candidates())
                    }
                    self.assertIn(repeated_move, native_candidates)
                    native_candidate = native_candidates[repeated_move]
                    self.assertGreater(native_candidate.visits, 0)
                    self.assertAlmostEqual(native_candidate.value, expected_value, places=6)

                    # Check the same candidate is a drawn repetition with the same Python
                    # correction.
                    board = player.get_board()
                    board.play(*repeated_move)
                    self.assertEqual(player.referee.judge(board),
                                     (True, COLOR_NONE, RESULT_SENNICHITE))
                    candidates = {
                        (c.src, c.dst, c.promote): c
                        for c in player.evaluate(64, timelimit=0.0, equally=True)
                    }
                    candidate = candidates[repeated_move]
                    self.assertAlmostEqual(candidate.value, expected_value, places=6)
                    self.assertAlmostEqual(candidate.value, native_candidate.value, places=6)
                    self.assertAlmostEqual(candidate.win_chance, 0.5 - max(penalty, 0.0) * 0.5)

    def test_stop_before_first_visit_and_restart(self) -> None:
        '''Interrupt first-visit waits and clear stopping state for the next search.
        '''
        player = Player(
            self.processor, max_visits=1_000_000_000, check_node_depth=0,
            check_search_depth=1)
        self.assertEqual(player.get_visits(), 0)
        self._assert_wait_is_interrupted(player)

        # Start a new search with the same player and verify actual evaluation results.
        candidates = player.evaluate(2, timelimit=1.0)
        self.assertGreater(player.get_visits(), 0)
        self.assertTrue(candidates)
        legal_moves = player.get_board().get_legal_moves()
        self.assertIn(
            (candidates[0].src, candidates[0].dst, candidates[0].promote), legal_moves)

    def test_stop_timed_wait(self) -> None:
        '''Interrupt timed waits before timeout after the initial evaluation.
        '''
        player = Player(
            self.processor, max_visits=1_000_000_000, check_node_depth=0,
            check_search_depth=1)
        player.evaluate(2, timelimit=1.0)
        player.native.start_evaluation(False, 0, 1.0, 0.0)
        self._assert_wait_is_interrupted(player)

    def test_usi_stop_and_next_go(self) -> None:
        '''Handle readiness and stop during actual search, then process the next go.
        '''
        # Drive CPU inference and C++ search through queued engine commands.
        player = Player(
            self.processor, max_visits=1_000_000_000, check_node_depth=1,
            check_search_depth=1)
        engine = USIEngine(
            self.processor, threads=1, visits=1_000_000_000,
            max_visits=1_000_000_000, timelimit=60.0, initial_turn=0)
        engine.player = player
        commands: Queue[str] = Queue()
        output: list[str] = []
        condition = Condition()
        started = Event()
        original = player.evaluate

        def evaluate(visits: int, **kwargs: Any) -> list[Candidate]:
            '''Return candidates for visits (int) and search arguments (kwargs: Any).
            Returns:
                list[Candidate]: Candidate moves returned by the real player search.
            '''
            started.set()
            return original(visits, **kwargs)

        def write(message: str) -> int:
            '''Record message (str) and return its character count (int).
            Returns:
                int: Number of characters in the recorded message.
            '''
            with condition:
                output.extend(message.splitlines())
                condition.notify_all()
            return len(message)

        engine.reader = Mock()
        engine.reader.readline.side_effect = commands.get
        engine.writer = Mock()
        engine.writer.write.side_effect = write
        worker = Thread(target=engine.run, daemon=True)
        with patch.object(player, 'evaluate', side_effect=evaluate) as run_evaluation:
            worker.start()
            try:
                commands.put('go byoyomi 60000\n')
                self.assertTrue(started.wait(1.0))
                commands.put('isready\n')
                with condition:
                    self.assertTrue(condition.wait_for(lambda: 'readyok' in output, 1.0))
                commands.put('stop\n')
                with condition:
                    self.assertTrue(condition.wait_for(
                        lambda: any(line.startswith('bestmove ') for line in output), 2.0))
                self.assertEqual(run_evaluation.call_count, 1)

                # Ignore stop after responding and start new searches only for new requests.
                commands.put('stop\n')
                commands.put('go byoyomi 1000\n')
                with condition:
                    self.assertTrue(condition.wait_for(
                        lambda: sum(line.startswith('bestmove ') for line in output) == 2, 2.0))
                self.assertEqual(run_evaluation.call_count, 2)
                legal_moves = set(player.get_board().get_legal_moves())
                for line in output:
                    if line.startswith('bestmove '):
                        self.assertIn(usi_string_to_move(line.split()[1]), legal_moves)
            finally:
                commands.put('quit\n')
                worker.join(3.0)
            self.assertFalse(worker.is_alive())
