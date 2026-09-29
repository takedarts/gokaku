'''CPU regressions for the model format and native player interface.'''

import io
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import Mock, patch

import numpy as np
import torch

from deepshogi.board import Board
from deepshogi.config import (
    COLOR_BLACK, COLOR_WHITE, MODEL_INFO_OFFSET, MODEL_INPUT_PACK_SIZE,
    MODEL_INPUT_SIZE, MODEL_OUTPUT_PACK_SIZE, MODEL_OUTPUT_SIZE, MODEL_VALUE_OFFSET,
)
from deepshogi.native import NativeInferenceModel
from deepshogi.player import Candidate
from deepshogi.processor import Processor
from deepshogi.usi import USIEngine
from run import parse_args


class ProbeModel(torch.nn.Module):
    '''Expose input features and predictable values without a trained model dependency.'''

    def forward(self, inputs: torch.Tensor) -> torch.Tensor:
        '''Map inputs (Tensor) to output features and game values (Tensor).'''
        # Echo all input features so native unpacking errors remain visible.
        policy = torch.cat((inputs, inputs), dim=1)[:, :MODEL_VALUE_OFFSET]
        info = inputs[:, MODEL_INFO_OFFSET:]
        value = torch.sigmoid(info[:, 77:78] + info[:, 81:82])
        remaining = info[:, 81:82] * 0.1 + 0.25
        return torch.cat((policy, value, value * 0, remaining, info[:, :6]), dim=1)


def unpack(inputs: np.ndarray) -> np.ndarray:
    '''Decode packed inputs (ndarray) into model features (ndarray).'''
    # Decode the binary region independently using NumPy, then restore scalar features.
    bits = ((inputs[:, :-5, None].astype(np.uint32) >> np.arange(32)) & 1)
    expanded = bits.reshape(len(inputs), -1)[:, :MODEL_INPUT_SIZE].astype(np.float32)
    expanded[:, MODEL_INFO_OFFSET + 77:MODEL_INFO_OFFSET + 82] = inputs[:, -5:] / 0xfffff
    return expanded


class InferenceTest(unittest.TestCase):
    '''Check the new layout through Python, Cython, and LibTorch on CPU.'''

    def test_input_and_inference(self) -> None:
        '''Check unpacking, rule inputs, and evaluation perspective; return None.'''
        boards = [Board(), Board(nyugyoku_scores=(31, 31)), Board(draw_turn=10)]
        white = Board()
        white.play(*white.get_legal_moves()[0])
        boards.append(white)
        packed = np.stack([board.get_inputs() for board in boards])
        self.assertEqual(packed.shape, (4, MODEL_INPUT_PACK_SIZE))
        self.assertEqual(MODEL_INPUT_SIZE, 6816)
        self.assertEqual(MODEL_OUTPUT_SIZE, 13293)
        self.assertEqual(Board().get_score(COLOR_BLACK), 27)
        self.assertEqual(Board().get_score(COLOR_WHITE), 27)
        self.assertEqual(Board().get_score(nyugyoku=True), 0)
        expected_scalars = np.array([0.05, -0.05, -0.05, -0.05, 0.0])
        np.testing.assert_allclose(packed[0, -5:] / 0xfffff, expected_scalars, atol=1e-6)

        # Compare native unpacking and masks with an independent Python evaluation.
        model = ProbeModel().eval()
        expanded = torch.tensor(unpack(packed))
        expected = model(expanded).detach().numpy()
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / 'probe.pt'
            torch.jit.trace(model, expanded[:1]).save(str(path))
            native = NativeInferenceModel(str(path), -1, False, True)
            masks = np.full((len(boards), MODEL_OUTPUT_PACK_SIZE), -1, dtype=np.int32)
            np.testing.assert_allclose(native.forward(packed, masks), expected, atol=1e-6)
            processor = Processor(path, [-1], False, True, 4, 1, 2)
            np.testing.assert_allclose(processor.execute(packed), expected, atol=1e-6)
            values = expected[:, MODEL_VALUE_OFFSET] * 2 - 1
            np.testing.assert_allclose(processor.evaluate(boards), values, atol=1e-6)

            # Native predict reconstructs SFEN with default rules; test that API on default boards.
            for index in (0, 0, 3, 3):
                board = boards[index]
                self.assertAlmostEqual(
                    processor.native.predict(board.native),
                    float(values[index]) * board.get_color(), places=5)
            self.assertGreater(processor.get_cache_hit_rate(), 0.0)

            # Run threaded lifecycle checks in a child so a deadlock has a firm timeout.
            for worker in ('search_worker.py', 'usi_worker.py'):
                result = subprocess.run(
                    [sys.executable, str(Path(__file__).with_name(worker)), str(path)],
                    capture_output=True, text=True, timeout=45)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_candidate(self) -> None:
        '''Check the new candidate tuple and nonnegative remaining moves; return None.'''
        candidate = Candidate((0, 0), (0, 1), False, COLOR_BLACK, 1, 0.5, 0.0, -1.0, [])
        self.assertEqual(candidate.remaining_turns, 0.0)
        self.assertFalse(hasattr(candidate, 'playouts'))

    def test_usi_and_arguments(self) -> None:
        '''Check new options and rejection of the removed playout argument; return None.'''
        with patch.object(sys, 'argv', [
            'run.py', 'probe.pt', '--gpus=-1', '--visits', '100', '--max-visits', '20',
            '--extends', '2', '--initial-value-delta', '0.02', '--initial-white-only',
            '--pucb-min-visits-rate', '0.2',
        ]):
            args = parse_args()
        self.assertEqual((args.max_visits, args.cache_size), (100, 100))
        self.assertEqual(args.extends, 2)
        self.assertTrue(args.initial_white_only)
        engine = USIEngine(Mock(), threads=1)
        for option in ('Extends', 'MaxVisits', 'InitialValueDelta',
                       'InitialWhiteOnly', 'PucbMinVisitsRate'):
            self.assertIn(option, engine.options)
        self.assertNotIn('Playouts', engine.options)
        with patch.object(sys, 'argv', ['run.py', 'probe.pt', '--playouts', '1']), \
                patch('sys.stderr', new_callable=io.StringIO), self.assertRaises(SystemExit):
            parse_args()
