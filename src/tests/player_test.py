import math
import random
import tempfile
import unittest
from pathlib import Path
from queue import Queue
from threading import Condition, Event, Thread
from typing import Any, ClassVar
from unittest.mock import Mock, PropertyMock, patch

import numpy as np

from deepshogi.board import Board
from deepshogi.config import (
    BOARD_SIZE, COLOR_BLACK, COLOR_NONE, COLOR_WHITE, DEFAULT_DRAW_TURN, DEFAULT_INITIAL_SFEN,
    RESULT_MAX_MOVES, RESULT_NONE, RESULT_NYUGYOKU, RESULT_SENNICHITE,
)
from deepshogi.player import Candidate, Player, Referee
from deepshogi.processor import Processor
from deepshogi.usi import USIEngine, usi_string_to_move
from tests.native import run_native_test
from tests.probe_model import SearchModel, save_compiled_probe


class RefereeTest(unittest.TestCase):
    '''Check repetition and perpetual-check judgments using the game history.'''

    def test_perpetual_check_in_both_phases(self) -> None:
        '''The side that keeps checking loses whether the cycle ends with a check or an evasion.
        '''
        for attacker, sfen, rank, king_rank in (
                (COLOR_BLACK, '4k4/9/9/5R3/9/9/9/9/4K4 b - 1', 3, 0),
                (COLOR_WHITE, '4k4/9/9/9/9/5r3/9/9/4K4 w - 1', 5, 8)):
            moves = [((3, rank), (4, rank), False), ((4, king_rank), (3, king_rank), False),
                     ((4, rank), (3, rank), False), ((3, king_rank), (4, king_rank), False)]
            for final_check in (False, True):
                for repetitions in (1, 3):
                    with self.subTest(attacker=attacker, check=final_check, repeats=repetitions):
                        # To end with a check, start the cycle right after the first check.
                        board = Board(sfen)
                        line = moves
                        if final_check:
                            board.play(*moves[0])
                            line = moves[1:] + moves[:1]
                        turns = line * repetitions
                        referee = Referee(
                            allowed_repeats=repetitions, draw_turn=board.get_turn() + len(turns))
                        for move in turns[:-1]:
                            self.assertFalse(referee.judge(board)[0])
                            referee.update(board)
                            board.play(*move)

                        # Do not change the history for a trial move, and prefer the perpetual
                        # check over the maximum move count.
                        next_board = Board()
                        next_board.copy_from(board)
                        next_board.play(*turns[-1])
                        expected = (True, -attacker, RESULT_SENNICHITE)
                        history = referee.check_history.copy()
                        self.assertEqual(referee.judge(next_board, previous_board=board), expected)
                        self.assertEqual(referee.check_history, history)
                        referee.update(board)
                        self.assertEqual(referee.judge(next_board), expected)

    def test_interrupted_check_in_first_cycle(self) -> None:
        '''A check interrupted in the first cycle of the repetition makes a normal repetition.
        '''
        # Move both kings first, then let Black keep checking for the remaining two cycles.
        board = Board('4k4/9/9/5R3/9/9/9/9/4K4 b - 1')
        referee = Referee()
        for move in ('5i6i', '5a6a', '6i5i', '6a5a',
                     '4d5d', '5a4a', '5d4d', '4a5a',
                     '4d5d', '5a4a', '5d4d', '4a5a'):
            referee.update(board)
            board.play(*usi_string_to_move(move))
        self.assertEqual(referee.judge(board), (True, COLOR_NONE, RESULT_SENNICHITE))


class PlayerTest(unittest.TestCase):
    '''Check candidates and search termination conditions of the Player class.'''

    processor: ClassVar[Processor]

    @classmethod
    def setUpClass(cls) -> None:
        '''Prepare a small CPU model and an inference processor shared by the tests.
        '''
        # Keep the model file until class cleanup because native loading is asynchronous.
        directory = tempfile.TemporaryDirectory()
        cls.addClassCleanup(directory.cleanup)
        path = save_compiled_probe(Path(directory.name), 'player_test', SearchModel(), 1)
        cls.processor = Processor(path, gpus=[-1], deterministic=True, batch_size=1)
        cls.addClassCleanup(delattr, cls, 'processor')

    def test_candidate_remaining_turns(self) -> None:
        '''Check the predicted remaining moves of a candidate do not become negative.
        '''
        # Create a candidate with negative predicted remaining moves.
        candidate = Candidate(
            (0, 0), (0, 1), False, COLOR_BLACK,
            1, 0.5, 0.0, -1.0, [],
        )

        # Check the remaining moves are corrected to zero and the old field is removed.
        self.assertEqual(candidate.remaining_turns, 0.0)
        self.assertFalse(hasattr(candidate, 'playouts'))

    def test_principal_variation_ratio_limit(self) -> None:
        '''Check the search ends by the visits of the principal candidate.
        '''
        # Limit the candidates to one move and evaluate until the maximum visits exceed 60%
        # of the target.
        player = Player(processor=self.processor, threads=1)
        candidates = player.evaluate(
            visits=100,
            candidate_width=1,
        )

        # Check the search continues while equal to 60% and ends after exceeding it.
        self.assertGreater(max(c.visits for c in candidates), 60)
        self.assertLess(sum(c.visits for c in candidates), 99)

    def test_visits_limit(self) -> None:
        '''Check the search ends by the visits of the root node.
        '''
        # Search candidates equally so the visit target is met before the 60% condition.
        player = Player(processor=self.processor, threads=1)
        candidates = player.evaluate(
            visits=100,
            equally=True,
            candidate_width=16,
        )

        # Check the visit condition is met before reaching the candidate condition.
        self.assertGreaterEqual(sum(c.visits for c in candidates), 99)
        self.assertLessEqual(max(c.visits for c in candidates), 60)

    def test_max_visits_precedes_evaluation_limits(self) -> None:
        '''Check the maximum visits take precedence over the normal search conditions.
        '''
        # Evaluate with a maximum visit count smaller than the normal search conditions.
        player = Player(
            processor=self.processor,
            threads=1,
            max_visits=4,
        )
        candidates = player.evaluate(
            visits=100,
            candidate_width=1,
        )

        # Check the search ends before reaching the normal search conditions.
        self.assertLess(sum(c.visits for c in candidates), 99)
        self.assertLess(max(c.visits for c in candidates), 61)


class PlayerExtendsTest(unittest.TestCase):
    '''Check search extension visit targets and termination conditions.'''

    def test_evaluate_judges_perpetual_check_on_evasion(self) -> None:
        '''Evaluate an evasion completing a perpetual check as a win and keep referee history.
        '''
        board = Board('4k4/9/5R3/9/9/9/9/9/4K4 b - 1')
        moves = [usi_string_to_move(move) for move in ('4c5c', '5a4a', '5c4c', '4a5a')] * 3
        player = object.__new__(Player)
        player.referee = Referee()
        player.native = Mock()
        player.processor = Mock()
        player.sennichite_penalty = 0.2

        # Play up to the last evasion and fix the native candidate value to a drawn repetition.
        for move in moves[:-1]:
            player.referee.update(board)
            board.play(*move)
        player.native.get_candidates.return_value = [
            (*moves[-1], COLOR_WHITE, 1, 1.0, 0.2, 0.0, [])]
        checks = player.referee.check_history.copy()
        repeats = player.referee.board_repeats.copy()

        # Judge the trial move including the previous check and override with the winner's sign.
        with patch.object(Player, 'get_board', side_effect=lambda: Board(board.get_sfen())):
            candidate = player.evaluate(2)[0]
        self.assertEqual(candidate.value, COLOR_WHITE)
        self.assertEqual(candidate.value_lcb, COLOR_WHITE)
        self.assertEqual(player.referee.check_history, checks)
        self.assertEqual(player.referee.board_repeats, repeats)

    def test_evaluate_extends(self) -> None:
        '''Check extension control using mocked results and time.
        '''
        # Combine visit targets, ranking criteria, candidates, and time boundaries.
        cases: list[tuple[
            str, int, int, str, list[tuple[int, float]], float, bool, list[int]
        ]] = [
            ('default', 100, 0, 'visits', [(50, 0.0), (50, 0.0)], 10.0, False,
             [100]),
            ('equal_half', 100, 2, 'visits', [(50, 0.0), (50, 0.0)], 10.0, False,
             [100, 150, 200]),
            ('majority', 100, 2, 'visits', [(51, 0.0), (49, 0.0)], 10.0, False,
             [100, 150, 200]),
            ('two_thirds', 150, 1, 'visits', [(90, 0.0), (60, 0.0)], 10.0, False,
             [150, 225]),
            ('below_two_thirds', 150, 1, 'visits', [(90, 0.0), (59, 0.0)], 10.0, False,
             [150]),
            ('fractional_threshold', 10, 1, 'visits', [(5, 0.0), (3, 0.0)], 10.0, False,
             [10]),
            ('dispersed', 100, 1, 'visits', [(40, 0.0), (20, 0.0), (20, 0.0), (20, 0.0)],
             10.0, False, [100]),
            ('single_zero', 100, 2, 'visits', [(0, 0.0)], 10.0, False,
             [100]),
            ('single_visited', 100, 2, 'visits', [(10, 0.0)], 10.0, False,
             [100]),
            ('value', 100, 1, 'value', [(70, -0.5), (30, 0.5)], 10.0, False,
             [100, 150]),
            ('value_third_candidate', 150, 1, 'value', [(90, 0.8), (10, 0.5), (60, -0.5)],
             10.0, False, [150, 225]),
            ('minimum_increment', 1, 2, 'visits', [(0, 0.0), (0, 0.0)], 10.0, False,
             [1, 2, 3]),
            ('fixed_increment', 5, 2, 'visits', [(0, 0.0), (0, 0.0)], 10.0, False,
             [5, 7, 9]),
            ('empty', 100, 2, 'visits', [], 10.0, False, [100]),
            ('short_time', 100, 2, 'visits', [(0, 0.0), (0, 0.0)], 0.9, False, [100]),
            ('one_second', 100, 1, 'visits', [(0, 0.0), (0, 0.0)], 1.0, False, [100, 150]),
            ('ponder', 100, 1, 'visits', [(0, 0.0), (0, 0.0)], 10.0, True, [100, 150]),
            ('negative', 100, -1, 'visits', [(0, 0.0), (0, 0.0)], 10.0, False, [100]),
        ]
        for name, visits, extends, criterion, stats, limit, ponder, targets in cases:
            with self.subTest(name=name):
                # Mock native search to exercise the extension control independently.
                player = object.__new__(Player)
                player.native = Mock()
                player.processor = Mock()
                player.referee = Mock()
                player.referee.judge.return_value = (False, COLOR_NONE, RESULT_NONE)
                player.native.get_candidates.return_value = [
                    ((0, 0), (0, 1), False, COLOR_BLACK, count, 0.5, value, 1.0, [])
                    for count, value in stats
                ]
                with patch.object(Player, 'get_board', return_value=Mock()), \
                        patch('deepshogi.player.time.monotonic', return_value=0.0), \
                        patch('deepshogi.player.LOGGER') as logger:
                    logger.isEnabledFor.return_value = False
                    candidates = player.evaluate(
                        visits, timelimit=limit, extends=extends,
                        criterion=criterion, ponder=ponder)

                # Check cumulative targets with fixed increments and pondering stop flags.
                calls = player.native.wait_evaluation.call_args_list
                self.assertEqual([c.args for c in calls], [
                    (target, limit, not ponder) for target in targets
                ])
                self.assertEqual(player.native.start_evaluation.call_count, len(targets))
                self.assertEqual(len(candidates), len(stats))

    def test_win_chance_boundaries(self) -> None:
        '''Check extension decisions at and around win-probability boundaries.
        '''
        # Satisfy the visit condition to isolate the win-probability condition.
        for win_chance, searches in (
            (0.049, 1), (0.05, 1), (0.051, 2),
            (0.949, 2), (0.95, 1), (0.951, 1),
        ):
            with self.subTest(win_chance=win_chance):
                player = object.__new__(Player)
                player.native = Mock()
                player.processor = Mock()
                player.referee = Mock()
                player.referee.judge.return_value = (False, COLOR_NONE, RESULT_NONE)
                player.native.get_candidates.return_value = [
                    ((0, 0), (0, 1), False, COLOR_BLACK, 50, 0.5, 0.0, 1.0, []),
                    ((1, 0), (1, 1), False, COLOR_BLACK, 50, 0.5, 0.0, 1.0, []),
                ]

                # Use exact probabilities to avoid rounding during evaluation conversion.
                with patch.object(Player, 'get_board', return_value=Mock()), \
                        patch('deepshogi.player.time.monotonic', return_value=0.0), \
                        patch.object(Candidate, 'win_chance', new_callable=PropertyMock,
                                     return_value=win_chance):
                    candidates = player.evaluate(100, extends=1)

                self.assertEqual(player.native.start_evaluation.call_count, searches)
                self.assertEqual(player.native.wait_evaluation.call_count, searches)
                self.assertEqual(len(candidates), 2)

    def test_win_chance_after_extension(self) -> None:
        '''Check termination when an extension moves the win probability outside the range.
        '''
        # Change the selected probability to zero or one only after the extension.
        for value in (-1.0, 1.0):
            with self.subTest(value=value):
                player = object.__new__(Player)
                player.native = Mock()
                player.processor = Mock()
                player.referee = Mock()
                player.referee.judge.return_value = (False, COLOR_NONE, RESULT_NONE)
                player.native.get_candidates.side_effect = [
                    [
                        ((0, 0), (0, 1), False, COLOR_BLACK, 60, 0.5, score, 1.0, []),
                        ((1, 0), (1, 1), False, COLOR_BLACK, 50, 0.5, 0.0, 1.0, []),
                    ]
                    for score in (0.0, value)
                ]
                with patch.object(Player, 'get_board', return_value=Mock()), \
                        patch('deepshogi.player.time.monotonic', return_value=0.0):
                    candidates = player.evaluate(100, extends=3, criterion='visits')

                # Stop on the probability condition and return the latest results despite spare
                # time.
                calls = player.native.wait_evaluation.call_args_list
                self.assertEqual([c.args[0] for c in calls], [100, 150])
                self.assertEqual(candidates[0].win_chance, (value + 1.0) / 2.0)

    def test_elapsed_time_and_latest_candidates(self) -> None:
        '''Check time accounting and returning candidates from the latest extension.
        '''
        # Leave one second after the first search and half a second after the extension.
        player = object.__new__(Player)
        player.native = Mock()
        player.processor = Mock()
        player.referee = Mock()
        player.referee.judge.return_value = (False, COLOR_NONE, RESULT_NONE)
        variations: list[int] = []
        candidate = ((0, 0), (0, 1), False, COLOR_BLACK, 0, 1.0, 0.0, 1.0, variations)
        player.native.get_candidates.side_effect = [
            [candidate, candidate], [candidate, candidate, candidate]]
        with patch.object(Player, 'get_board', return_value=Mock()), \
                patch('deepshogi.player.time.monotonic',
                      side_effect=[0.0, 0.0, 0.2, 9.0, 9.2, 9.5]):
            candidates = player.evaluate(100, timelimit=10.0, extends=3)

        # Check that search startup time is also deducted from the wait budget.
        calls = player.native.wait_evaluation.call_args_list
        self.assertEqual([c.args[0] for c in calls], [100, 150])
        self.assertAlmostEqual(calls[0].args[1], 9.8)
        self.assertAlmostEqual(calls[1].args[1], 0.8)
        self.assertEqual(len(candidates), 3)


class PlayerRandomCandidateTest(unittest.TestCase):
    '''Check random selection probabilities using arbitrary policies and win probabilities.'''

    def _make_player(self, policies: list[float], values: list[float] | None = None) -> Player:
        '''Create a player with fixed search results.
        Args:
            policies (list[float]): Policy for each candidate.
            values (list[float] | None): Candidate values from Black's perspective (default: zero).
        Returns:
            Player: Player with mocked native search.
        '''
        # Use legal pawn moves and set boundary values without model inference.
        if values is None:
            values = [0.0] * len(policies)
        player = object.__new__(Player)
        player.native = Mock()
        player.referee = Referee()
        player.native.get_candidates.return_value = [
            ((i, 6), (i, 5), False, COLOR_BLACK, 1, policy, value, 1.0, [])
            for i, (policy, value) in enumerate(zip(policies, values))
        ]
        return player

    def test_invalid_delta_rejected_before_evaluation(self) -> None:
        '''Check invalid win probability tolerances are rejected before searching.
        '''
        for delta in (-0.01, float('nan'), float('inf'), -float('inf')):
            # Fail with the argument name before getting candidates or waiting for evaluation.
            with self.subTest(delta=delta):
                player = self._make_player([1.0])
                with self.assertRaisesRegex(ValueError, 'delta'):
                    player.get_random_candidate(delta=delta)
                self.assertEqual(player.native.mock_calls, [])

    def test_delta_includes_win_chance_boundary(self) -> None:
        '''Check only candidates within the tolerance of the best win probability remain.
        '''
        player = self._make_player([0.25] * 4, [0.5, 0.5, 0.25, 0.0])

        # Include candidates tied for the best and those exactly at the tolerance.
        for delta, expected in ((0.0, [0.75, 0.75]), (0.125, [0.75, 0.75, 0.625])):
            with self.subTest(delta=delta), \
                    patch('deepshogi.player.random.choices', wraps=random.choices) as choices:
                player.get_random_candidate(delta=delta)
                self.assertEqual([c.win_chance for c in choices.call_args.args[0]], expected)

    def test_low_temperature_keeps_equal_probabilities(self) -> None:
        '''Check equal positive weights for equal policies at zero and near-minimum temperatures.
        '''
        for temperature in (0.0, 0.0001, 0.001):
            with self.subTest(temperature=temperature):
                player = self._make_player([0.1, 0.1, 0.1])
                with patch('deepshogi.player.random.choices', wraps=random.choices) as choices:
                    candidate = player.get_random_candidate(temperature=temperature)
                    assert candidate is not None

                # Check selection succeeds and equal candidates receive equal weights.
                weights = choices.call_args.kwargs['weights']
                self.assertTrue(math.isfinite(sum(weights)))
                self.assertGreater(sum(weights), 0.0)
                self.assertEqual(weights, [weights[0]] * 3)
                self.assertIn(candidate.src, ((0, 6), (1, 6), (2, 6)))

    def test_tiny_policies_and_zero_policy(self) -> None:
        '''Check tiny positive policies remain selectable while zero policies keep zero weight.
        '''
        player = self._make_player([0.0, 1e-200, 2e-200])
        with patch('deepshogi.player.random.choices', wraps=random.choices) as choices:
            candidate = player.get_random_candidate(temperature=0.001)
            assert candidate is not None

        # Avoid losing all weights through underflow without making zero-policy candidates
        # selectable.
        weights = choices.call_args.kwargs['weights']
        self.assertEqual(weights[0], 0.0)
        self.assertTrue(math.isfinite(sum(weights)))
        self.assertGreater(sum(weights), 0.0)
        self.assertIn(candidate.src, ((1, 6), (2, 6)))

    def test_normal_temperature_preserves_distribution(self) -> None:
        '''Check normal temperatures preserve probabilities from the original power ratios.
        '''
        for temperature, policies in ((1.0, [0.1, 0.2, 0.3]), (2.0, [0.01, 0.04, 0.09])):
            with self.subTest(temperature=temperature):
                player = self._make_player(policies)
                with patch('deepshogi.player.random.choices', wraps=random.choices) as choices:
                    player.get_random_candidate(temperature=temperature)

                # Both distributions have a 1:2:3 ratio after temperature scaling, regardless of a
                # common factor.
                weights = choices.call_args.kwargs['weights']
                for weight, expected in zip(weights, (1 / 6, 1 / 3, 1 / 2)):
                    self.assertAlmostEqual(weight / sum(weights), expected, places=12)

    def test_zero_policies_are_uniform(self) -> None:
        '''Check all-zero policies permit uniform selection regardless of temperature.
        '''
        for temperature in (0.0, 1.0):
            with self.subTest(temperature=temperature):
                player = self._make_player([0.0, 0.0, 0.0])
                with patch('deepshogi.player.random.choices', wraps=random.choices) as choices:
                    candidate = player.get_random_candidate(temperature=temperature)
                    assert candidate is not None

                # Keep selection possible with equal positive weights when policies have no
                # preference.
                weights = choices.call_args.kwargs['weights']
                self.assertGreater(weights[0], 0.0)
                self.assertEqual(weights, [weights[0]] * 3)
                self.assertIn(candidate.src, ((0, 6), (1, 6), (2, 6)))

    def test_normalization_uses_filtered_candidates(self) -> None:
        '''Check normalization and uniform selection use only candidates passing the value filter.
        '''
        for remaining_policies in ([1e-200, 2e-200], [0.0, 0.0]):
            with self.subTest(remaining_policies=remaining_policies):
                # Filter out the largest policy by value, leaving only tiny or zero policies.
                player = self._make_player([0.9, *remaining_policies], [-1.0, 0.0, 0.0])
                with patch('deepshogi.player.random.choices', wraps=random.choices) as choices:
                    candidate = player.get_random_candidate(temperature=0.0)
                    assert candidate is not None

                # Select using valid weights of remaining candidates without restoring excluded
                # candidates.
                population = choices.call_args.args[0]
                weights = choices.call_args.kwargs['weights']
                self.assertEqual([c.src for c in population], [(1, 6), (2, 6)])
                self.assertTrue(math.isfinite(sum(weights)))
                self.assertGreater(sum(weights), 0.0)
                self.assertIn(candidate.src, ((1, 6), (2, 6)))
                if remaining_policies == [0.0, 0.0]:
                    self.assertEqual(weights[0], weights[1])

    def test_single_candidate_is_selectable(self) -> None:
        '''Check a sole candidate is selected for both tiny and zero policies.
        '''
        for policy in (0.0, 1e-200):
            with self.subTest(policy=policy):
                player = self._make_player([policy])
                candidate = player.get_random_candidate(temperature=0.0)
                assert candidate is not None
                self.assertEqual((candidate.src, candidate.dst, candidate.promote),
                                 ((0, 6), (0, 5), False))

    def _make_repetition_player(self, board: Board) -> Player:
        '''Create a player advanced to just before a repeated move in a perpetual-check line.
        Args:
            board (Board): Board advanced from the initial position.
        Returns:
            Player: Player keeping game history and replaceable search results.
        '''
        # Register positions before each move and replace the native board retrieval.
        player = self._make_player([])
        for move in ('4c5c', '5a4a', '5c4c'):
            player.referee.update(board)
            board.play(*usi_string_to_move(move))
        player.native.copy_board_to.side_effect = lambda dest: dest.copy_from(board.native)
        return player

    def test_repetition_is_excluded_before_win_chance_filter(self) -> None:
        '''Check moves returning to an earlier position are excluded before comparing values.
        '''
        board = Board('4k4/9/5R3/9/9/9/9/9/4K4 b - 1')
        player = self._make_repetition_player(board)
        repeated = usi_string_to_move('4a5a')
        other = usi_string_to_move('4a3a')
        player.native.get_candidates.return_value = [
            (*repeated, COLOR_WHITE, 10, 0.9, -1.0, 5.0, []),
            (*other, COLOR_WHITE, 2, 0.1, 0.0, 6.0, []),
        ]
        sfen = board.get_sfen()
        history = player.referee.board_repeats.copy()

        # Return the other search candidate with its visits even if the repeating move is best.
        candidate = player.get_random_candidate(delta=0.0)
        assert candidate is not None
        self.assertEqual((candidate.src, candidate.dst, candidate.promote), other)
        self.assertEqual(candidate.visits, 2)
        self.assertEqual(board.get_sfen(), sfen)
        self.assertEqual(player.referee.board_repeats, history)
        player.native.get_policy_moves.assert_not_called()

    def test_fallback_uses_highest_policy_that_does_not_repeat(self) -> None:
        '''Check a fallback is chosen from unexpanded policies when all candidates repeat.
        '''
        board = Board('4k4/9/5R3/9/9/9/9/9/4K4 b - 1')
        player = self._make_repetition_player(board)
        player.native.get_candidates.return_value = [
            (*usi_string_to_move('4a5a'), COLOR_WHITE, 1, 0.8, -1.0, 5.0, [])]

        # Pass policies in arbitrary order and exclude 4a5a, which escapes back to a repetition.
        policy_moves = []
        for move, probability in (('4a3a', 0.1), ('4a5a', 0.8), ('4a5b', 0.6)):
            (src_x, src_y), (dst_x, dst_y), _ = usi_string_to_move(move)
            src = src_x * BOARD_SIZE + src_y
            dst = dst_x * BOARD_SIZE + dst_y
            policy_moves.append(((src << 7) | dst, probability))
        player.native.get_policy_moves.return_value = (-0.2, 8.0, policy_moves)
        candidate = player.get_random_candidate(width=1)
        assert candidate is not None
        self.assertEqual((candidate.src, candidate.dst, candidate.promote),
                         usi_string_to_move('4a5b'))
        self.assertEqual((candidate.visits, candidate.value, candidate.remaining_turns),
                         (0, -0.2, 7.0))

    def test_no_nonrepeating_candidate(self) -> None:
        '''Check None is returned when every legal move returns to the game history.
        '''
        board = Board('4k4/9/9/9/9/9/9/9/4K4 b - 1')
        player = self._make_player([])
        player.native.copy_board_to.side_effect = lambda dest: dest.copy_from(board.native)
        player.native.get_policy_moves.return_value = (0.0, 0.0, [])

        # Register the positions after every legal move before selecting a candidate.
        for move in board.get_legal_moves(remove_unpromote=False):
            result = board.play(*move)
            player.referee.update(board)
            board.undo(*result)
        self.assertIsNone(player.get_random_candidate())


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
        board = Board()
        self.player.referee = Referee()
        self.player.native.copy_board_to.side_effect = lambda dest: dest.copy_from(board.native)
        self.player.native.get_candidates.return_value = []
        self.player.native.get_policy_moves.return_value = (0.0, 0.0, [])

        # Search for another legal move when the first legal move repeats a position.
        legal_moves = board.get_legal_moves(remove_unpromote=False)
        result = board.play(*legal_moves[0])
        self.player.referee.update(board)
        board.undo(*result)
        candidate = self.player.get_random_candidate(stop_event=self.stop_event)
        assert candidate is not None
        self.assertIn(
            (candidate.src, candidate.dst, candidate.promote), legal_moves[1:])
        self.player.native.get_candidates.assert_called_once_with()
        self.player.native.start_evaluation.assert_not_called()


class NativePlayerStopTest(unittest.TestCase):
    '''Verify actual C++ wait interruption and search restart after stopping.'''

    model_path: ClassVar[Path]
    processor: ClassVar[Processor]

    @classmethod
    def setUpClass(cls) -> None:
        '''Prepare a small CPU model and actual inference processor.
        '''
        # Create a PT2 readable by the current processor and keep it until class cleanup.
        directory = tempfile.TemporaryDirectory()
        cls.addClassCleanup(directory.cleanup)
        cls.model_path = save_compiled_probe(Path(directory.name), 'stop_test', SearchModel(), 1)
        cls.processor = Processor(
            cls.model_path, gpus=[-1], deterministic=True, batch_size=1, threads_per_gpu=1)
        cls.addClassCleanup(delattr, cls, 'processor')

    def test_cpp_player_wait(self) -> None:
        '''Run the native Player regressions using the CPU PT2 model.
        '''
        run_native_test('player_wait_native_test', str(self.model_path))

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

        # Do not count failed moves in position occurrences or check history.
        board = player.get_board()
        self.assertEqual(board.get_sfen(), initial_sfen)
        self.assertEqual(player.referee.judge(board), (False, COLOR_NONE, RESULT_NONE))
        self.assertEqual(player.referee.board_repeats, {})
        self.assertEqual(player.referee.check_history, {})

    def test_failed_play_preserves_referee_state(self) -> None:
        '''Check referee state survives failed moves and later legal moves update history.
        '''
        for attacking_color in (COLOR_BLACK, COLOR_WHITE):
            for checking in (False, True):
                with self.subTest(attacking_color=attacking_color, checking=checking):
                    # Check the king and let it escape so the position history contains a check.
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
                    check_history = player.referee.check_history.copy()
                    self.assertEqual(check_history[1], (attacking_color, True))

                    # Prevent failed moves from changing the check or position history.
                    with self.assertRaisesRegex(ValueError, 'Position does not have a piece'):
                        player.play((0, 4), (0, 3))
                    self.assertEqual(player.get_board().get_sfen(), board.get_sfen())
                    self.assertEqual(player.referee.board_repeats, repeats)
                    self.assertEqual(player.referee.check_history, check_history)

                    # Record the previous position exactly once after the next legal move.
                    expected_referee = Referee()
                    expected_referee.board_repeats = repeats
                    expected_referee.check_history = check_history
                    expected_referee.update(board)
                    move = board.get_legal_moves()[0]
                    self.assertEqual(player.play(*move), move)
                    board.play(*move)
                    self.assertEqual(player.get_board().get_sfen(), board.get_sfen())
                    self.assertEqual(player.referee.board_repeats,
                                     expected_referee.board_repeats)
                    self.assertEqual(player.referee.check_history, expected_referee.check_history)

    def test_failed_play_preserves_search_and_history(self) -> None:
        '''Check failed moves preserve board and history during active and stopped searches.
        '''
        for searching in (False, True):
            with self.subTest(searching=searching):
                # Create position history containing a check, then run native search.
                player = Player(
                    self.processor, initial_sfen='4k4/9/3R5/9/9/9/9/9/K8 b - 1',
                    threads=2, max_visits=1_000_000, check_node_depth=0)
                for src, dst in (((5, 2), (4, 2)), ((4, 0), (5, 0)), ((4, 2), (5, 2))):
                    player.play(src, dst)
                before = player.get_board()
                repeats = dict(player.referee.board_repeats)
                check_history = player.referee.check_history.copy()
                self.assertEqual(check_history[1], (COLOR_BLACK, True))
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
                        self.assertEqual(player.referee.check_history, check_history)

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
                expected_referee.check_history = check_history.copy()
                expected_referee.update(before)
                move = before.get_legal_moves()[0]
                self.assertEqual(player.play(*move), move)
                before.play(*move)
                self.assertEqual(player.get_board().get_sfen(), before.get_sfen())
                self.assertEqual(player.referee.board_repeats, expected_referee.board_repeats)
                self.assertEqual(player.referee.check_history, expected_referee.check_history)
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

        # Prepare position history containing a check and a retained search result.
        for src, dst in (((5, 2), (4, 2)), ((4, 0), (5, 0)), ((4, 2), (5, 2))):
            player.play(src, dst)
        self.assertTrue(player.evaluate(8, equally=True, timelimit=1.0))
        before = player.get_board()
        repeats = dict(player.referee.board_repeats)
        check_history = player.referee.check_history.copy()
        self.assertTrue(repeats)
        self.assertEqual(check_history[1], (COLOR_BLACK, True))
        invalid_turn = ' '.join(before.get_sfen().split()[:-1] + ['invalid'])

        for sfen, new_scores, error_type in (
            ('X8/9/9/9/9/9/9/9/8K b - 1', (56, 56), IndexError),
            (invalid_turn, (56, 56), ValueError),
            (before.get_sfen(), (56, 128), ValueError),
        ):
            with self.subTest(sfen=sfen, scores=new_scores):
                # Preserve original rules and history if initialization fails after Python stops
                # search.
                with self.assertRaises(error_type):
                    player.initialize(sfen, draw_turn=400, nyugyoku_scores=new_scores)
                after = player.get_board()
                self.assertEqual(after.get_sfen(), before.get_sfen())
                np.testing.assert_array_equal(after.get_inputs(), before.get_inputs())
                self.assertEqual(player.referee.board_repeats, repeats)
                self.assertEqual(player.referee.check_history, check_history)
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
        self.assertEqual(player.referee.check_history, {})
        self.assertEqual(player.referee.draw_turn, 256)
        self.assertEqual(player.get_visits(), 0)

    def test_initialize_nyugyoku_scores(self) -> None:
        '''Apply scores to nyugyoku judgment and model inputs on each override and restoration.
        '''
        # Prepare a position where Black can declare with 28 points after White's move.
        sfen = 'RR2K2BB/PPPPPP3/9/9/9/9/9/9/4k4 w 2G 1'
        player = Player(self.processor, nyugyoku_scores=(28, 29), check_node_depth=0)
        move = (4, 8), (3, 8), False

        # Check initialization without scores restores the construction scores on a reused root.
        for override, expected_inputs, expected_result in (
            (None, (0.05, 0.15), (True, COLOR_BLACK, RESULT_NYUGYOKU)),
            ((56, 56), (2.85, 2.85), (False, COLOR_NONE, RESULT_NONE)),
            (None, (0.05, 0.15), (True, COLOR_BLACK, RESULT_NYUGYOKU)),
        ):
            with self.subTest(scores=override):
                player.initialize(sfen, nyugyoku_scores=override)
                self.assertIn(move, player.get_board().get_legal_moves())
                player.play(*move)
                board = player.get_board()
                self.assertEqual(player.referee.judge(board), expected_result)

                # Check our and the opponent's required points relative to the side to move.
                scalars = board.get_inputs()[-5:-3] / 0xfffff
                np.testing.assert_allclose(scalars, expected_inputs, atol=1e-6)

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
