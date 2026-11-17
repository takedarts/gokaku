import math
import random
import unittest
from unittest.mock import Mock, patch

from deepshogi.config import COLOR_BLACK
from deepshogi.player import Player


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
        player.native.get_candidates.return_value = [
            ((i, 6), (i, 5), False, COLOR_BLACK, 1, policy, value, 1.0, [])
            for i, (policy, value) in enumerate(zip(policies, values))
        ]
        return player

    def test_low_temperature_keeps_equal_probabilities(self) -> None:
        '''Check equal positive weights for equal policies at zero and near-minimum temperatures.
        '''
        for temperature in (0.0, 0.0001, 0.001):
            with self.subTest(temperature=temperature):
                player = self._make_player([0.1, 0.1, 0.1])
                with patch('deepshogi.player.random.choices', wraps=random.choices) as choices:
                    candidate = player.get_random_candidate(temperature=temperature)

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
                self.assertEqual((candidate.src, candidate.dst, candidate.promote),
                                 ((0, 6), (0, 5), False))

