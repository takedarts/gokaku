import unittest
from unittest.mock import Mock, PropertyMock, patch

from deepshogi.config import COLOR_BLACK, COLOR_NONE, RESULT_NONE
from deepshogi.player import Candidate, Player


class PlayerExtendsTest(unittest.TestCase):
    '''Check search extension visit targets and termination conditions.'''

    def test_evaluate_extends(self) -> None:
        '''Check extension control using mocked results and time.
        Returns:
            None: No return value.
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
        Returns:
            None: No return value.
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
        Returns:
            None: No return value.
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
        Returns:
            None: No return value.
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
