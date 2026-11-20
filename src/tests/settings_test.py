import io
import math
import os
import subprocess
import sys
import tempfile
import unittest
from contextlib import redirect_stderr
from pathlib import Path
from typing import Any, ClassVar, cast
from unittest.mock import MagicMock, Mock, call, patch

import deepshogi.loader as loader
import run as engine_run
import deepshogi.native.modules as native_modules  # type: ignore[import-not-found]
from deepshogi.board import Board
from deepshogi.config import (DEFAULT_CHECK_SEARCH_DEPTH, DEFAULT_CHECK_SEARCH_NODE,
                              DEFAULT_NYUGYOKU_SCORES, DEFAULT_PUCB_CONSTANT_BASE,
                              DEFAULT_PUCB_CONSTANT_INIT, DEFAULT_PUCB_MIN_VISITS_RATE,
                              MODEL_OUTPUT_SIZE)
from deepshogi.gpu import get_default_gpus
from deepshogi.native import NativeInferenceProcessor, NativePlayer
from deepshogi.player import Player
from deepshogi.processor import Processor
from deepshogi.usi import USIEngine
from deepshogi.validation import INT32_MAX
from tests.probe_model import SearchModel, save_compiled_probe


# Include finite values that convert to zero or infinity as 32-bit floats.
INVALID_PUCB_BASES = (0.0, -0.0, -1.0, math.nan, math.inf, -math.inf, 1e-100, 1e100)
VALID_PUCB_BASES = (
    0.5, 1.0, DEFAULT_PUCB_CONSTANT_BASE,
    float.fromhex('0x1p-149'), float.fromhex('0x1.fffffep127'))


class SettingsTest(unittest.TestCase):
    '''Check every input path rejects invalid settings before search.'''

    def test_player_rejects_invalid_settings_before_native_creation(self) -> None:
        '''Check invalid search settings cannot construct a native player; return None.'''
        # Reject zero, negative, out-of-range, and noninteger values before conversion or
        # allocation.
        for name in ('threads', 'max_visits'):
            for value in (0, -1, INT32_MAX + 1, 1.5, True):
                with self.subTest(name=name, value=value), \
                        patch('deepshogi.player.NativePlayer') as native:
                    settings: dict[str, Any] = {name: value}
                    with self.assertRaises(ValueError):
                        Player(cast(Processor, Mock()), **settings)
                    native.assert_not_called()

    def test_player_rejects_invalid_pucb_base_before_native_creation(self) -> None:
        '''Reject invalid PUCB bases before native construction; return None.'''
        # Check native construction is not called, including for invalid types.
        for value in (*INVALID_PUCB_BASES, True, False, '1'):
            with self.subTest(value=value), patch('deepshogi.player.NativePlayer') as native:
                with self.assertRaisesRegex(ValueError, 'pucb_constant_base'):
                    Player(cast(Processor, Mock()), pucb_constant_base=cast(float, value))
                native.assert_not_called()

    def test_player_accepts_positive_pucb_base_and_zero_init(self) -> None:
        '''Allow positive fractional bases, boundary values, and a zero initial coefficient.; return
            None.'''
        # Preserve other settings' supported ranges and the supplied base value.
        for value in VALID_PUCB_BASES:
            with self.subTest(value=value), patch('deepshogi.player.NativePlayer') as native:
                Player(cast(Processor, Mock()), pucb_constant_base=value, pucb_constant_init=0.0)
                self.assertEqual(native.call_args.args[-3:-1], (0.0, value))

    def test_usi_rejects_invalid_initial_pucb_base(self) -> None:
        '''Reject PUCB bases unsupported by USI at construction; return None.'''
        # Restrict USI spin values to integers even though the API supports positive fractions.
        for value in (*INVALID_PUCB_BASES, 0.5, 1.5, True):
            with self.subTest(value=value), \
                    self.assertRaisesRegex(ValueError, 'pucb_constant_base'):
                USIEngine(cast(Processor, Mock()), threads=1, pucb_constant_base=value)
        for value in (1.0, DEFAULT_PUCB_CONSTANT_BASE):
            with self.subTest(value=value):
                engine = USIEngine(cast(Processor, Mock()), threads=1, pucb_constant_base=value)
                self.assertEqual(engine.pucb_constant_base, value)

    def test_usi_rejects_invalid_initial_value_delta(self) -> None:
        '''Reject invalid win probability tolerances in direct USI construction; return None.'''
        for value in (-0.01, math.nan, math.inf, -math.inf):
            # Reject the value before storing the option.
            with self.subTest(value=value), \
                    self.assertRaisesRegex(ValueError, 'initial_value_delta'):
                USIEngine(cast(Processor, Mock()), threads=1, initial_value_delta=value)

    def test_processor_rejects_invalid_settings_before_native_creation(self) -> None:
        '''Check invalid inference configurations cannot construct native processors.; return
            None.'''
        # Validate individual values and calculated thread, queue, and buffer sizes.
        configurations: list[dict[str, Any]] = [
            {'gpus': []}, {'gpus': [-2]}, {'gpus': [-1, INT32_MAX]},
            {'gpus': [False]}, {'gpus': [-1.0]},
            {'gpus': [-1, -1], 'threads_per_gpu': INT32_MAX // 2 + 1},
            {'threads_per_gpu': INT32_MAX // 10 + 1},
            {'batch_size': INT32_MAX // MODEL_OUTPUT_SIZE + 1},
        ]
        for name in ('batch_size', 'threads_per_gpu'):
            configurations.extend({name: value} for value in
                                  (0, -1, INT32_MAX + 1, 1.5, True))
        for settings in configurations:
            with self.subTest(settings=settings), \
                    patch('deepshogi.processor.NativeInferenceModel') as model, \
                    patch('deepshogi.processor.NativeInferenceProcessor') as native:
                model.get_available_gpus.return_value = [-1]
                with self.assertRaises(ValueError):
                    Processor(__file__, **settings)
                native.assert_not_called()

    def test_valid_settings_preserve_zero_options(self) -> None:
        '''Accept valid minimum settings and existing zero-valued options; return None.'''
        # Keep supporting disabled caches, no node mate search, and stops without waiting.
        with patch('deepshogi.processor.NativeInferenceModel') as model, \
                patch('deepshogi.processor.NativeInferenceProcessor') as native_processor, \
                patch('deepshogi.player.NativePlayer') as native_player:
            model.get_available_gpus.return_value = [-1]
            processor = Processor(__file__, gpus=[-1], batch_size=1,
                                  threads_per_gpu=1, cache_size=0)
            Player(processor, threads=1, max_visits=1, check_node_depth=0)
            native_processor.assert_called_once()
            native_player.assert_called_once()
            native_player.return_value.wait_evaluation.assert_called_once_with(0, 0.0, True)

    def test_device_selection_rejects_invalid_ids(self) -> None:
        '''Reject unavailable devices rather than removing them; return None.'''
        # Reject configurations with invalid IDs even if some devices are available.
        with patch('deepshogi.gpu.NativeInferenceModel') as model:
            model.get_available_gpus.return_value = [-1, 0]
            for gpus in ([], [-2], [1], [0, 1], [False], [-1.0]):
                with self.subTest(gpus=gpus), self.assertRaises(ValueError):
                    get_default_gpus(cast(list[int], gpus), False)

            # Preserve automatic device selection and disabling FP16 on CPU.
            self.assertEqual(get_default_gpus(None, True), ([0], True))
            self.assertEqual(get_default_gpus([-1], True), ([-1], False))
            self.assertEqual(get_default_gpus([0, -1], True), ([0, -1], False))

    def test_cpu_model_device_selection(self) -> None:
        '''Prefer the device and precision of a CPU model even on a GPU machine; return None.'''
        # Check both a model path and metadata that has already been read.
        path = Path('cpu.pt2')
        metadata = {'device_type': 'cpu', 'precision': 'fp16'}
        with patch('deepshogi.gpu.NativeInferenceModel') as native, \
                patch('deepshogi.gpu.read_model_metadata', return_value=metadata) as read:
            native.get_available_gpus.return_value = [-1, 0, 1]
            self.assertEqual(get_default_gpus(None, model=path), ([-1], True))
            self.assertEqual(get_default_gpus([-1], model=metadata), ([-1], True))
            with self.assertRaises(ValueError):
                get_default_gpus([1], model=metadata)
            read.assert_called_once_with(path)

    def test_gpu_model_device_selection(self) -> None:
        '''Select only configurations whose GPU IDs and runtime both match; return None.'''
        # CUDA and ROCm share IDs, so also compare the stored runtime.
        platforms = [('cuda', 'cuda', [0, 1]), ('cuda', 'rocm', [0, 1]), ('mps', 'metal', [0])]
        for device, runtime, ids in platforms:
            with self.subTest(runtime=runtime), \
                    patch('deepshogi.gpu.NativeInferenceModel') as native, \
                    patch('torch.cuda.is_available', return_value=device == 'cuda'), \
                    patch('torch.version.hip', '7' if runtime == 'rocm' else None), \
                    patch('torch.backends.mps.is_available', return_value=device == 'mps'):
                native.get_available_gpus.return_value = [-1, *ids]
                metadata = {'device_type': device, 'gpu_runtime': runtime, 'precision': 'fp32'}
                self.assertEqual(get_default_gpus(None, True, metadata), (ids, False))
                for selected in (ids[::-1], ids[-1:]):
                    self.assertEqual(get_default_gpus(selected, True, metadata), (selected, False))
                for invalid in ([-1], [-1, 0], [0, 2]):
                    with self.assertRaises(ValueError):
                        get_default_gpus(invalid, model=metadata)

                # Reject models compiled for another GPU even with automatic selection.
                for other_device, other_runtime, _ in platforms:
                    if other_runtime != runtime:
                        with self.assertRaises(ValueError):
                            get_default_gpus(None, model=dict(metadata, device_type=other_device,
                                                             gpu_runtime=other_runtime))

        # Do not run a GPU model on the CPU when no GPU is available.
        with patch('torch.cuda.is_available', return_value=False), \
                patch('torch.backends.mps.is_available', return_value=False), \
                self.assertRaises(ValueError):
            get_default_gpus(None, model=metadata)

    def test_default_ep_devices(self) -> None:
        '''Select all GPUs of the same model as the first one for an EP by default; return None.
        '''
        # Use the CPU without GPUs and every GPU when only one GPU or MPS is available.
        for available, cuda, expected in (
                ([-1], False, [-1]), ([-1, 0], True, [0]), ([-1, 0], False, [0])):
            with self.subTest(available=available, cuda=cuda), \
                    patch('deepshogi.loader.NativeInferenceModel') as native, \
                    patch('torch.cuda.is_available', return_value=cuda):
                native.get_available_gpus.return_value = available
                self.assertEqual(loader._get_ep_gpus(None), expected)

        # Exclude CUDA GPUs whose model differs from the first one and log a warning.
        names = {0: 'A', 1: 'B', 2: 'A'}
        with patch('deepshogi.loader.NativeInferenceModel') as native, \
                patch('torch.cuda.is_available', return_value=True), \
                patch('torch.cuda.get_device_name', side_effect=names.__getitem__), \
                self.assertLogs('deepshogi.loader', level='WARNING') as logs:
            native.get_available_gpus.return_value = [-1, 0, 1, 2]
            self.assertEqual(loader._get_ep_gpus(None), [0, 2])
        self.assertIn('[1]', logs.output[0])

    def test_open_processor_forwards_ep_compile_settings(self) -> None:
        '''Pass EP compile settings and validate sizes before compiling; return None.'''
        metadata = {'device_type': 'cpu', 'precision': 'fp16', 'batch_size': '8'}
        with patch('deepshogi.loader._get_model_format', return_value='ep'), \
                patch('compile.save_compiled_model') as compile_model, \
                patch('deepshogi.loader.read_model_metadata', return_value=metadata), \
                patch('deepshogi.loader.Processor') as processor:
            with loader.open_processor(
                    'model.ep', gpus=[-1], threads_per_gpu=1, backend='aoti', fp16=True,
                    batch_size=8):
                pass
            self.assertEqual(compile_model.call_args.kwargs, {
                'backend': 'aoti', 'gpu': -1, 'fp16': True, 'batch_size': 8})
            self.assertEqual(processor.call_args.kwargs['batch_size'], 8)

            # Reject a batch size overflowing the buffers without starting the compilation.
            compile_model.reset_mock()
            with self.assertRaises(ValueError):
                with loader.open_processor('model.ep', gpus=[-1], batch_size=INT32_MAX):
                    pass
            compile_model.assert_not_called()

    def test_run_rejects_invalid_settings_before_correction(self) -> None:
        '''Reject invalid CLI values before normalization; return None.'''
        # Prevent a zero maximum visit count from being normalized to a positive target.
        for option in ('--threads', '--max-visits', '--threads-per-gpu'):
            for value in ('0', '-1', str(INT32_MAX + 1)):
                with self.subTest(option=option, value=value), \
                        patch('sys.argv', ['run.py', __file__, option, value]), \
                        redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as error:
                    engine_run.parse_args()
                self.assertEqual(error.exception.code, 2)

        # Validate removed options, device ID syntax, and normalized visits at startup.
        for options in (['--fp16'], ['--batch-size', '1'], ['--gpus', '1,1'], ['--gpus', '0,'],
                        ['--visits', str(INT32_MAX + 1)]):
            with self.subTest(options=options), patch('sys.argv', ['run.py', 'a.pt2', *options]), \
                    redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as error:
                engine_run.parse_args()
            self.assertEqual(error.exception.code, 2)

    def test_initial_value_delta_arguments(self) -> None:
        '''Check the default and input range of the win probability tolerance; return None.'''
        argv = ['run.py', 'model.ep']

        # Parse the default, zero restricting to the best candidates, and a positive fraction.
        for options, expected in (
            ((), 0.01), (('--initial-value-delta', '0'), 0.0),
            (('--initial-value-delta', '0.125'), 0.125),
        ):
            with self.subTest(options=options), patch('sys.argv', [*argv, *options]):
                self.assertEqual(engine_run.parse_args().initial_value_delta, expected)

        # Check the parse error names the tolerance option.
        for value in ('-0.01', 'nan', 'inf', '-inf'):
            with self.subTest(value=value), \
                    patch('sys.argv', [*argv, f'--initial-value-delta={value}']), \
                    redirect_stderr(io.StringIO()) as stderr:
                with self.assertRaises(SystemExit) as error:
                    engine_run.parse_args()
                self.assertEqual(error.exception.code, 2)
                self.assertIn('--initial-value-delta', stderr.getvalue().splitlines()[-1])

    def test_run_defers_model_settings_until_isready(self) -> None:
        '''Open the PT2 at isready, apply its stored conditions, and reuse it; return None.'''
        metadata = {'device_type': 'cpu', 'precision': 'fp16', 'batch_size': '3'}
        argv = ['run.py', 'cpu.pt2', '--visits', '2', '--max-visits', '1', '--threads-per-gpu', '1']

        def run_commands(engine: USIEngine) -> None:
            '''Check the order of responses and loading in the USI loop.
            Args:
                engine (USIEngine): Engine registered with the production factory.
            '''
            # Do not open the model before the readiness check.
            self.assertIn('usiok', engine._perform_command('usi')[1])
            read.assert_not_called()
            processor.assert_not_called()

            # Load once with the stored execution conditions even on repeated readiness checks.
            for _ in range(2):
                self.assertEqual(engine._perform_command('isready'), (True, 'readyok', False))
            self.assertEqual(engine.max_visits, 2)
            read.assert_called_once_with(Path('cpu.pt2'))
            processor.assert_called_once_with(
                model=Path('cpu.pt2'), gpus=[-1], fp16=True, batch_size=3,
                threads_per_gpu=1, cache_size=2)

        with patch('sys.argv', argv), patch('run.start_logging'), \
                patch.object(USIEngine, 'run', autospec=True, side_effect=run_commands), \
                patch('deepshogi.loader._get_model_format', return_value='pt2'), \
                patch('deepshogi.loader.read_model_metadata', return_value=metadata) as read, \
                patch('deepshogi.loader.Processor') as processor, patch('deepshogi.usi.Player'):
            engine_run.main()

    def test_run_rejects_incompatible_inference_settings_at_isready(self) -> None:
        '''Detect device mismatches and size overflow at isready without a processor.; return
            None.'''
        for gpu_ids, batch_size in [('1,0', '3'), ('-1', str(INT32_MAX))]:
            metadata = {'device_type': 'cpu', 'precision': 'fp32', 'batch_size': batch_size}
            argv = ['run.py', 'cpu.pt2', '--gpus', gpu_ids]

            def run_commands(engine: USIEngine) -> None:
                '''Validate the model execution conditions at the readiness check.
                Args:
                    engine (USIEngine): Engine registered with the production factory.
                '''
                # Do not match the environment at startup, and return the isready failure.
                native.assert_not_called()
                with self.assertRaises(ValueError):
                    engine._perform_command('isready')
                self.assertIsNone(engine.processor)
                native.assert_not_called()

            with self.subTest(gpu_ids=gpu_ids), patch('sys.argv', argv), \
                    patch('run.start_logging'), \
                    patch.object(USIEngine, 'run', autospec=True, side_effect=run_commands), \
                    patch('deepshogi.loader._get_model_format', return_value='pt2'), \
                    patch('deepshogi.loader.read_model_metadata', return_value=metadata), \
                    patch('deepshogi.processor.NativeInferenceProcessor') as native:
                engine_run.main()

    def test_run_retries_ep_and_closes_after_usi_loop(self) -> None:
        '''Reopen at the next isready after an EP failure and close when the loop ends; return
            None.'''
        failed_loader = MagicMock()
        failed_loader.__enter__.side_effect = RuntimeError('compile failed')
        loader = MagicMock()

        def run_commands(engine: USIEngine) -> None:
            '''Check the error and the retry in the same USI loop.
            Args:
                engine (USIEngine): Engine that loads the EP lazily.
            '''
            self.assertIn('usiok', engine._perform_command('usi')[1])
            open_model.assert_not_called()
            with self.assertRaisesRegex(RuntimeError, 'compile failed'):
                engine._perform_command('isready')
            self.assertIsNone(engine.processor)
            for _ in range(2):
                self.assertEqual(engine._perform_command('isready'), (True, 'readyok', False))
            self.assertIs(engine.processor, loader.__enter__.return_value)
            loader.__exit__.assert_not_called()

        argv = ['run.py', 'cpu.ep', '--gpus=-1', '--threads-per-gpu', '1', '--cache-size', '4']
        with patch('sys.argv', argv), patch('run.start_logging'), \
                patch('deepshogi.usi.Player'), \
                patch('run.open_processor', side_effect=[failed_loader, loader]) as open_model, \
                patch.object(USIEngine, 'run', autospec=True, side_effect=run_commands):
            engine_run.main()

        self.assertEqual(open_model.call_args_list, [
            call('cpu.ep', gpus=[-1], threads_per_gpu=1, cache_size=4),
            call('cpu.ep', gpus=[-1], threads_per_gpu=1, cache_size=4),
        ])
        loader.__exit__.assert_called_once()

    def test_usi_rejects_invalid_initial_settings(self) -> None:
        '''Reject invalid search settings when constructing the USI engine; return None.'''
        # Do not retain invalid settings before receiving isready.
        for name in ('threads', 'max_visits'):
            for value in (0, -1, INT32_MAX + 1):
                settings: dict[str, Any] = {'threads': 1, name: value}
                with self.subTest(settings=settings), self.assertRaises(ValueError):
                    USIEngine(cast(Processor, Mock()), **settings)

    def test_usi_keeps_previous_value_after_invalid_option(self) -> None:
        '''Reject invalid USI settings and accept the next valid setting; return None.'''
        # Preserve settings after out-of-range or unconvertible values.
        engine = USIEngine(cast(Processor, Mock()), threads=2, max_visits=100)
        for option, name in (('Threads', 'threads'), ('MaxVisits', 'max_visits')):
            previous = getattr(engine, name)
            for value in ('0', '-1', str(INT32_MAX + 1), '1.5', 'abc'):
                with self.subTest(option=option, value=value):
                    result = engine._perform_command(f'setoption name {option} value {value}')
                    self.assertTrue(result[1])
                    self.assertEqual(getattr(engine, name), previous)
            self.assertEqual(
                engine._perform_command(f'setoption name {option} value 1'), (False, '', False))
            self.assertEqual(getattr(engine, name), 1)


class NativeSettingsTest(unittest.TestCase):
    '''Check direct native calls reject invalid values and support CPU search.'''

    processor: ClassVar[Processor]
    model_path: ClassVar[Path]
    player_settings: ClassVar[dict[str, Any]]

    @classmethod
    def setUpClass(cls) -> None:
        '''Prepare a small CPU model and native constructor arguments; return None.'''
        # Keep the model file alive until all asynchronous inference ends.
        directory = tempfile.TemporaryDirectory()
        cls.addClassCleanup(directory.cleanup)
        path = save_compiled_probe(Path(directory.name), 'settings_test', SearchModel(), 1)
        cls.model_path = path
        cls.processor = Processor(path, gpus=[-1], batch_size=1, threads_per_gpu=1)
        cls.addClassCleanup(delattr, cls, 'processor')
        cls.player_settings = {
            'threads': 1, 'max_visits': 1, 'nyugyoku_scores': DEFAULT_NYUGYOKU_SCORES,
            'sennichite_penalty': 0.0, 'check_search_depth': DEFAULT_CHECK_SEARCH_DEPTH,
            'check_search_node': DEFAULT_CHECK_SEARCH_NODE, 'check_node_depth': 0,
            'pucb_constant_init': DEFAULT_PUCB_CONSTANT_INIT,
            'pucb_constant_base': DEFAULT_PUCB_CONSTANT_BASE,
            'pucb_min_visits_rate': DEFAULT_PUCB_MIN_VISITS_RATE,
        }

    def test_native_player_rejects_invalid_settings(self) -> None:
        '''Reject invalid values when constructing NativePlayer directly; return None.'''
        # Exercise calls that bypass the Python Player wrapper.
        for name in ('threads', 'max_visits'):
            for value in (0, -1, INT32_MAX + 1):
                settings = dict(self.player_settings, **{name: value})
                with self.subTest(settings=settings), self.assertRaises(ValueError):
                    NativePlayer(self.processor.native, **settings)

    def test_native_player_rejects_invalid_pucb_base(self) -> None:
        '''Reject invalid PUCB bases in direct NativePlayer calls; return None.'''
        # Reject values that would overflow float before Cython converts them.
        for value in INVALID_PUCB_BASES:
            settings = dict(self.player_settings, pucb_constant_base=value)
            with self.subTest(value=value), \
                    self.assertRaisesRegex(ValueError, 'pucb_constant_base'):
                NativePlayer(self.processor.native, **settings)

    def test_native_player_rejects_nonfinite_mcts_settings(self) -> None:
        '''Reject nonfinite floating-point settings before search starts; return None.'''
        # Reach C++ constructor validation through both Player and NativePlayer.
        for name in ('sennichite_penalty', 'pucb_constant_init', 'pucb_min_visits_rate'):
            for value in (math.nan, math.inf, -math.inf, 1e100):
                settings = dict(self.player_settings, **{name: value})
                python_settings: dict[str, Any] = {name: value}
                with self.subTest(name=name, value=value):
                    with self.assertRaisesRegex(ValueError, name):
                        NativePlayer(self.processor.native, **settings)
                    with self.assertRaisesRegex(ValueError, name):
                        Player(self.processor, **python_settings)

    def test_cpu_search_with_positive_pucb_base(self) -> None:
        '''Check searches with positive PUCB bases through Python and native APIs; return None.'''
        # Search beyond the first visit so PUCB selects child nodes.
        for value in VALID_PUCB_BASES:
            for direct_native in (False, True):
                with self.subTest(value=value, direct_native=direct_native):
                    settings = dict(self.player_settings, max_visits=16, pucb_constant_base=value)
                    if direct_native:
                        native = NativePlayer(self.processor.native, **settings)
                    else:
                        player = Player(self.processor, threads=1, max_visits=16,
                                        check_node_depth=0, pucb_constant_base=value)
                        native = player.native
                    native.start_evaluation(False, 0, 1.0, 0.0)
                    native.wait_evaluation(16, 5.0, True)
                    self.assertGreaterEqual(native.get_visits(), 16)
                    self.assertTrue(native.get_candidates())

    def test_usi_search_with_positive_pucb_base(self) -> None:
        '''Check USI option changes through actual search and response generation; return None.'''
        # Set minimum and default bases via USI and return PUCB search results.
        engine = USIEngine(self.processor, threads=1, visits=16, max_visits=16,
                           check_node_depth=0, initial_turn=0, timelimit=5.0)
        for value in (1, int(DEFAULT_PUCB_CONSTANT_BASE)):
            with self.subTest(value=value):
                result = engine._perform_command(f'setoption name PucbConstantBase value {value}')
                self.assertEqual(result, (False, '', False))
                self.assertEqual(engine._perform_command('isready'), (True, 'readyok', False))
                engine._perform_command('position startpos')
                output, message, _ = engine._perform_command('go')
                self.assertTrue(output)
                self.assertIn('bestmove ', message)
                self.assertIsNotNone(engine.player)
                assert engine.player is not None
                self.assertGreaterEqual(engine.player.get_visits(), 16)
                engine._perform_command('gameover draw')

    def test_native_processor_rejects_invalid_settings(self) -> None:
        '''Reject invalid settings in direct NativeInferenceProcessor construction; return None.'''
        # Use nonexistent model paths for settings that must fail before model loading.
        configurations: list[dict[str, Any]] = [
            {'gpus': []}, {'gpus': [-2]}, {'gpus': [-1, INT32_MAX]},
            {'batch_size': 0}, {'batch_size': -1}, {'batch_size': INT32_MAX + 1},
            {'threads_per_gpu': 0}, {'threads_per_gpu': -1},
            {'threads_per_gpu': INT32_MAX},
            {'batch_size': INT32_MAX // MODEL_OUTPUT_SIZE + 1},
        ]
        for invalid in configurations:
            settings: dict[str, Any] = {
                'gpus': [-1], 'fp16': False, 'deterministic': True,
                'batch_size': 1, 'threads_per_gpu': 1, 'cache_size': 0,
            }
            settings.update(invalid)
            with self.subTest(settings=settings), self.assertRaises(ValueError):
                NativeInferenceProcessor('unused.model', **settings)

    def test_cpu_search_with_minimum_settings(self) -> None:
        '''Check minimum valid settings still return candidates; return None.'''
        # Use actual CPU inference with one maximum visit and no node mate search.
        player = Player(self.processor, threads=1, max_visits=1, check_node_depth=0)
        candidates = player.evaluate(1, timelimit=1.0)
        self.assertTrue(candidates)
        self.assertGreaterEqual(player.get_visits(), 1)

    def test_native_player_keeps_processor_alive(self) -> None:
        '''Check inference, cache use, and garbage collection after owner references are released.;
            return None.'''
        # Use a separate process to detect destruction order failures.
        script = '''
import gc
import sys
import weakref
from deepshogi.config import (DEFAULT_INITIAL_SFEN, DEFAULT_NYUGYOKU_SCORES,
                              DEFAULT_PUCB_CONSTANT_INIT, DEFAULT_PUCB_CONSTANT_BASE,
                              DEFAULT_PUCB_MIN_VISITS_RATE)
from deepshogi.native import NativePlayer
from deepshogi.player import Player
from deepshogi.processor import Processor

processor = Processor(sys.argv[1], gpus=[-1], batch_size=1, threads_per_gpu=1, cache_size=64)
native = NativePlayer(processor.native, 1, 100, DEFAULT_NYUGYOKU_SCORES, 0.0,
                      1, 1, 0, DEFAULT_PUCB_CONSTANT_INIT, DEFAULT_PUCB_CONSTANT_BASE,
                      DEFAULT_PUCB_MIN_VISITS_RATE)
del processor
gc.collect()

# Evaluate the same position repeatedly, including synchronous cache-hit callbacks.
for _ in range(3):
    native.initialize(DEFAULT_INITIAL_SFEN, 512)
    native.start_evaluation(False, 0, 1.0, 0.0)
    native.wait_evaluation(8, 2.0, True)
    assert native.get_candidates()

# Keep the processor alive when cyclic garbage collection destroys the Cython player.
native.start_evaluation(False, 0, 1.0, 0.0)
cycle = [native]
cycle.append(cycle)
del native, cycle
gc.collect()

# Check that cleanup also does not depend on Python Player attribute release order.
processor = Processor(sys.argv[1], gpus=[-1], batch_size=1, threads_per_gpu=1)
player = Player(processor, threads=1, check_node_depth=0)
reference = weakref.ref(player)
player.native.start_evaluation(False, 0, 1.0, 0.0)
player.cycle = player
del processor, player
gc.collect()
assert reference() is None
'''
        result = subprocess.run(
            [sys.executable, '-B', '-c', script, str(self.model_path)],
            env={**os.environ, 'PYTHONPATH': os.pathsep.join(sys.path)},
            capture_output=True, text=True, timeout=20)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_parallel_processor_initialization_failure_cleans_up(self) -> None:
        '''Recreate processors after some or all parallel creations fail; return None.'''
        # Check failure handling in a separate process to detect hangs and crashes.
        script = '''
import math
from pathlib import Path
import sys
import tempfile
from unittest.mock import patch
from deepshogi.board import Board
from deepshogi.native import NativeInferenceProcessor
from deepshogi.native import modules

model_path = sys.argv[1]

# Collect the exception in either device order even when some workers have been created.
with patch.object(modules, 'validate_device_ids'):
    for devices in ([-1, 2147483647], [2147483647, -1]):
        try:
            NativeInferenceProcessor(model_path, devices, False, True, 1, 1, 0)
        except ValueError:
            pass
        else:
            raise AssertionError('invalid device was accepted')

# Return the exception to Python even when every task fails to load the model.
with tempfile.TemporaryDirectory() as directory:
    broken_path = Path(directory) / 'broken.model'
    broken_path.write_bytes(b'invalid model')
    for path in (broken_path, Path(directory) / 'missing.model'):
        try:
            NativeInferenceProcessor(str(path), [-1, -1], False, True, 1, 1, 0)
        except RuntimeError:
            pass
        else:
            raise AssertionError('invalid model was accepted')

# After the failures, create, run, and destroy multiple executors in the same process.
processor = NativeInferenceProcessor(model_path, [-1, -1], False, True, 1, 1, 0)
assert math.isfinite(processor.predict(Board().native))
del processor
'''
        result = subprocess.run(
            [sys.executable, '-B', '-c', script, str(self.model_path)],
            env={**os.environ, 'PYTHONPATH': os.pathsep.join(sys.path)},
            capture_output=True, text=True, timeout=20)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_cpp_handles_settings_without_python_validation(self) -> None:
        '''Bypass Python validation and check native normalization and rejection; return None.'''
        # Normalize player settings and verify actual search with the same processor.
        with patch.object(native_modules, 'validate_positive_int'), \
                patch.object(native_modules, 'validate_inference_sizes'), \
                patch.object(native_modules, 'validate_device_ids'):
            for name in ('threads', 'max_visits'):
                for value in (0, -1):
                    settings = dict(self.player_settings, **{name: value})
                    with self.subTest(settings=settings):
                        native = NativePlayer(self.processor.native, **settings)
                        native.start_evaluation(False, 0, 1.0, 0.0)
                        native.wait_evaluation(1, 5.0, True)
                        self.assertGreaterEqual(native.get_visits(), 1)
                        self.assertTrue(native.get_candidates())

            # Normalize inference batch and thread counts in C++ and verify inference still works.
            for name in ('batch_size', 'threads_per_gpu'):
                for value in (0, -1):
                    settings = {
                        'gpus': [-1], 'fp16': False, 'deterministic': True,
                        'batch_size': 1, 'threads_per_gpu': 1, 'cache_size': 0,
                        name: value,
                    }
                    with self.subTest(settings=settings):
                        inference = NativeInferenceProcessor(str(self.model_path), **settings)
                        self.assertTrue(math.isfinite(inference.predict(Board().native)))

            # Reject invalid sizes and devices during initialization and join started workers.
            configurations: list[dict[str, Any]] = [
                {'gpus': []}, {'gpus': [INT32_MAX]}, {'gpus': [-1, INT32_MAX]},
                {'gpus': [-1, -1], 'threads_per_gpu': INT32_MAX},
                {'batch_size': INT32_MAX // MODEL_OUTPUT_SIZE + 1},
            ]
            for invalid in configurations:
                settings = {
                    'gpus': [-1], 'fp16': False, 'deterministic': True,
                    'batch_size': 1, 'threads_per_gpu': 1, 'cache_size': 0,
                }
                settings.update(invalid)
                with self.subTest(settings=settings), self.assertRaises(ValueError):
                    NativeInferenceProcessor(str(self.model_path), **settings)
