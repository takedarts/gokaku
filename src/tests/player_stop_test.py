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
    COLOR_BLACK, COLOR_NONE, DEFAULT_DRAW_TURN, DEFAULT_INITIAL_SFEN,
    MODEL_INPUT_SIZE, MODEL_VALUE_OFFSET, RESULT_MAX_MOVES,
)
from deepshogi.player import Candidate, Player
from deepshogi.processor import Processor
from deepshogi.usi import USIEngine, usi_string_to_move


class StopModel(torch.nn.Module):
    '''Provide deterministic CPU search outputs without a trained model.'''

    def forward(self, inputs: torch.Tensor) -> torch.Tensor:
        '''Map input features (Tensor) to policy and game value outputs (Tensor).'''
        # Match the native output layout using a single input feature.
        value = torch.sigmoid(inputs[:, :1])
        policy = value.expand(-1, MODEL_VALUE_OFFSET)
        return torch.cat((policy, value, value * 0, value * 0 + 1, inputs[:, :6]), dim=1)


class PlayerStopTest(unittest.TestCase):
    '''Verify Python stop notifications and suppression of subsequent searches.'''

    def setUp(self) -> None:
        '''Prepare a player with mocked native search.
        Returns:
            None: No return value.
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
        Returns:
            None: No return value.
        '''
        self.stop_event.set()
        self.assertEqual(self.player.evaluate(100, stop_event=self.stop_event), [])
        self.player.native.start_evaluation.assert_not_called()
        self.player.native.wait_evaluation.assert_not_called()

    def test_stop_during_start(self) -> None:
        '''Notify stopping after startup when it races with search startup.
        Returns:
            None: No return value.
        '''
        # Simulate native startup clearing the stopped state.
        self.player.native.start_evaluation.side_effect = lambda *args: self.stop_event.set()
        result = self.player.evaluate(100, extends=3, stop_event=self.stop_event)
        self.assertEqual(len(result), 2)
        self.player.native.stop_evaluation.assert_called_once()
        self.assertEqual(self.player.native.start_evaluation.call_count, 1)

    def test_stop_prevents_extensions(self) -> None:
        '''Return available candidates without extensions when stopped during a result wait.
        Returns:
            None: No return value.
        '''
        self.player.native.wait_evaluation.side_effect = lambda *args: self.stop_event.set()
        result = self.player.evaluate(100, extends=3, stop_event=self.stop_event)
        self.assertEqual(len(result), 2)
        self.assertEqual(self.player.native.start_evaluation.call_count, 1)
        self.assertEqual(self.player.native.wait_evaluation.call_count, 1)

    def test_random_stop_before_first_evaluation(self) -> None:
        '''Return a legal random fallback when stopped before the first evaluation.
        Returns:
            None: No return value.
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
        Returns:
            None: No return value.
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

    def test_initialize_draw_turn(self) -> None:
        '''Keep referee and native draw limits consistent after reinitialization and moves.
        Returns:
            None: No return value.
        '''
        # Check default draw limits and entering-king rules immediately after construction.
        scores = (31, 31)
        player = Player(self.processor, nyugyoku_scores=scores, check_node_depth=0)
        expected = Board(nyugyoku_scores=scores)
        np.testing.assert_array_equal(player.get_board().get_inputs(), expected.get_inputs())
        self.assertEqual(player.referee.draw_turn, DEFAULT_DRAW_TURN)

        # Reuse one player and restore the default when the draw limit is omitted.
        for configured_turn in (256, 300, None, 256):
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
        Returns:
            None: No return value.
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

    def test_stop_before_first_visit_and_restart(self) -> None:
        '''Interrupt first-visit waits and clear stopping state for the next search.
        Returns:
            None: No return value.
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
        Returns:
            None: No return value.
        '''
        player = Player(
            self.processor, max_visits=1_000_000_000, check_node_depth=0,
            check_search_depth=1)
        player.evaluate(2, timelimit=1.0)
        player.native.start_evaluation(False, 0, 1.0, 0.0)
        self._assert_wait_is_interrupted(player)

    def test_usi_stop_and_next_go(self) -> None:
        '''Handle readiness and stop during actual search, then process the next go.
        Returns:
            None: No return value.
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
            '''Return candidates for visits (int) and search arguments (kwargs: Any).'''
            started.set()
            return original(visits, **kwargs)

        def write(message: str) -> int:
            '''Record message (str) and return its character count (int).'''
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
