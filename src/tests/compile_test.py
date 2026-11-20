'''Check PT2 compilation plumbing without requiring a CUDA runtime.'''

import importlib
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from typing import Any, ClassVar
from unittest.mock import Mock, patch

import torch
import torch._inductor
from torch.export import ExportedProgram

from deepshogi.config import MODEL_INPUT_SIZE
from deepshogi.gpu import read_model_metadata
from tests.probe_model import EchoModel, save_exported_probe

compiler = importlib.import_module('compile')

MODEL_NAME = 'echo'
CPU = torch.device('cpu')


def get_signature(exported: ExportedProgram) -> list[tuple[str, str, str]]:
    '''Get the sequence of operations, constant arguments, and output shapes and types.
    Args:
        exported (ExportedProgram): Model to compare.
    Returns:
        list[tuple[str, str, str]]: Sequence of operation names, arguments, and outputs.
    '''
    signature = []

    for node in exported.graph.nodes:
        if node.op != 'call_function':
            continue

        # Type check nodes record arguments differently, so compare only the checked type.
        if node.target == torch.ops.aten._assert_tensor_metadata.default:
            dtype = node.kwargs.get('dtype', node.args[3] if len(node.args) > 3 else None)
            signature.append((str(node.target), str(dtype), ''))
            continue

        # Input node names differ between paths, so compare only constant arguments.
        args = [arg if not isinstance(arg, torch.fx.Node) else '%' for arg in node.args]
        value = node.meta.get('val')
        output = (tuple(value.shape), value.dtype) if isinstance(value, torch.Tensor) else None
        signature.append((str(node.target), str(args), f'{node.kwargs} {output}'))

    return signature


class CompileTest(unittest.TestCase):
    '''Check fixed-condition models are prepared from a dynamic-batch EP without the model
    definitions.'''

    directory: ClassVar[tempfile.TemporaryDirectory[str]]
    model: ClassVar[EchoModel]
    dynamic_path: ClassVar[Path]
    static_path: ClassVar[Path]
    inputs: ClassVar[torch.Tensor]

    @classmethod
    def setUpClass(cls) -> None:
        '''Create dynamic-batch and static-batch EPs from a small model.'''
        cls.directory = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.directory.cleanup)
        directory = Path(cls.directory.name)

        # Put the model in evaluation mode.
        cls.model = EchoModel().eval()

        # Save a dynamic-batch EP in the same format as the upstream exporter.
        cls.dynamic_path = directory / 'dynamic.ep'
        save_exported_probe(cls.dynamic_path, cls.model, 2, dynamic=True, model_name=MODEL_NAME)

        # Save a static-batch EP in the former format with batch size 2.
        cls.inputs = (torch.rand(8, MODEL_INPUT_SIZE) < 0.1).float()
        cls.static_path = directory / 'static.ep'
        save_exported_probe(cls.static_path, cls.model, 2, model_name=MODEL_NAME)

    def prepare(self, path: Path, batch_size: int, dtype: torch.dtype) -> Any:
        '''Load an EP and prepare a model for the specified conditions on the CPU.
        Args:
            path (Path): Path of the input EP.
            batch_size (int): Fixed batch size.
            dtype (torch.dtype): FP32 or FP16.
        Returns:
            Any: Pair of the prepared model and its example input.
        '''
        exported, _ = compiler.load_exported_model(path)

        with torch.no_grad():
            return compiler.prepare_exported_model(
                exported, batch_size=batch_size, device=CPU, dtype=dtype)

    def reference(self, dtype: torch.dtype) -> EchoModel:
        '''Create a copy of the original model converted to the specified precision.
        Args:
            dtype (torch.dtype): Precision of the copy.
        Returns:
            EchoModel: Model with the same weights in the specified precision.
        '''
        model = EchoModel()
        model.load_state_dict(self.model.state_dict())
        return model.to(dtype=dtype).eval()

    def test_dynamic_export(self) -> None:
        '''The EP has a dynamic batch dimension and stores no fixed batch in the metadata.'''
        exported, metadata = compiler.load_exported_model(self.dynamic_path)

        # Check only the batch dimension was exported as dynamic.
        self.assertTrue(compiler.has_dynamic_batch(exported))
        self.assertNotIn('batch_size', metadata)
        self.assertEqual(metadata['schema_version'], 2)
        self.assertEqual(metadata['model_name'], MODEL_NAME)

        # A static-batch EP is not judged as dynamic.
        static, _ = compiler.load_exported_model(self.static_path)
        self.assertFalse(compiler.has_dynamic_batch(static))

    def test_prepare_dynamic(self) -> None:
        '''Models with any precision and fixed batch can be prepared from a dynamic-batch EP.'''
        for dtype in (torch.float32, torch.float16):
            for batch_size in (1, 3):
                with self.subTest(dtype=dtype, batch_size=batch_size):
                    prepared, example = self.prepare(self.dynamic_path, batch_size, dtype)

                    # Check the input shape and precision of the prepared model are fixed.
                    self.assertFalse(compiler.has_dynamic_batch(prepared))
                    self.assertFalse(prepared.range_constraints)
                    self.assertEqual(example.shape, (batch_size, MODEL_INPUT_SIZE))
                    self.assertEqual(example.dtype, dtype)

                    # No FP32 type specification remains in the graph after converting to FP16.
                    if dtype == torch.float16:
                        texts = [f'{n.args}{n.kwargs}' for n in prepared.graph.nodes]
                        self.assertFalse(any('float32' in text for text in texts))

                    # Compare outputs with the original model converted to the same precision.
                    inputs = self.inputs[:batch_size].to(dtype)

                    with torch.no_grad():
                        actual = prepared.module()(inputs)
                        expected = self.reference(dtype)(inputs)

                    torch.testing.assert_close(actual, expected, atol=1e-5, rtol=1e-5)

    def test_prepare_matches_fixed_export(self) -> None:
        '''The prepared graph matches the original model exported with a fixed batch.'''
        for dtype in (torch.float32, torch.float16):
            with self.subTest(dtype=dtype):
                prepared, example = self.prepare(self.dynamic_path, 4, dtype)

                # Export the original model with the specified precision and fixed batch.
                with torch.no_grad():
                    expected = torch.export.export(
                        self.reference(dtype), (example,), strict=False)

                # Check operations, constant arguments, output shapes, and types match.
                self.assertEqual(get_signature(prepared), get_signature(expected))

    def test_prepare_static(self) -> None:
        '''A static-batch EP can be prepared only with the same precision and batch.'''
        prepared, example = self.prepare(self.static_path, 2, torch.float32)
        self.assertEqual(example.shape, (2, MODEL_INPUT_SIZE))
        self.assertFalse(prepared.range_constraints)

        # Changing the batch or the precision is an error.
        for batch_size, dtype in ((3, torch.float32), (2, torch.float16)):
            with self.subTest(batch_size=batch_size, dtype=dtype):
                with self.assertRaisesRegex(ValueError, 'static-batch'):
                    self.prepare(self.static_path, batch_size, dtype)

    def test_save_compiled_model(self) -> None:
        '''A dynamic-batch EP is compiled with AOTI and saved as a fixed-batch PT2.'''
        with tempfile.TemporaryDirectory() as directory:
            package = Path(directory) / 'model.pt2'
            compiler.save_compiled_model(
                self.dynamic_path, package, backend='aoti', gpu=-1, batch_size=3)

            # Record the compile conditions and the model name in the PT2 metadata.
            metadata = read_model_metadata(package)
            self.assertEqual(metadata['batch_size'], '3')
            self.assertEqual(metadata['precision'], 'fp32')
            self.assertEqual(metadata['model_name'], MODEL_NAME)

            # Compare the compiled model outputs with the original model.
            runner = torch._inductor.aoti_load_package(str(package))

            with torch.no_grad():
                actual = runner(self.inputs[:3])
                expected = self.model(self.inputs[:3])

            torch.testing.assert_close(actual, expected, atol=1e-4, rtol=1e-4)

    def test_save_static_mismatch(self) -> None:
        '''Changing the batch of a static-batch EP is an error without creating an output.'''
        with tempfile.TemporaryDirectory() as directory:
            package = Path(directory) / 'model.pt2'

            with self.assertRaisesRegex(ValueError, 'static-batch'):
                compiler.save_compiled_model(
                    self.static_path, package, backend='aoti', gpu=-1, batch_size=3)

            self.assertFalse(package.exists())

    def test_no_model_dependency(self) -> None:
        '''Changing the precision and batch does not import the model definition module.'''
        # Check the dependency in a separate process so test imports do not interfere.
        script = '\n'.join([
            'import sys, torch',
            'from compile import load_exported_model, prepare_exported_model',
            'exported, _ = load_exported_model(sys.argv[1])',
            'prepare_exported_model(exported, batch_size=3, device=torch.device("cpu"),',
            '                       dtype=torch.float16)',
            'assert "deepshogi.model" not in sys.modules',
        ])
        source_path = Path(__file__).parent.parent
        result = subprocess.run(
            [sys.executable, '-c', script, str(self.dynamic_path)],
            cwd=source_path, capture_output=True, text=True, check=False)

        self.assertEqual(result.returncode, 0, result.stderr)


class CompileSettingsTest(unittest.TestCase):
    '''Check device and backend selection, metadata, and file handling of the compiler.'''

    def test_resolve_device(self) -> None:
        '''Select CUDA/ROCm, MPS, or the CPU and reject unavailable indices; return None.'''
        with patch.object(torch.cuda, 'is_available', return_value=False), \
                patch.object(torch.backends.mps, 'is_available', return_value=False):
            self.assertEqual(compiler.resolve_device(None), torch.device('cpu'))
            self.assertEqual(compiler.resolve_device(-1), torch.device('cpu'))
            for gpu in (-2, 0):
                with self.subTest(gpu=gpu), self.assertRaises(ValueError):
                    compiler.resolve_device(gpu)

        # Specify the MPS index explicitly and reject other indices.
        with patch.object(torch.cuda, 'is_available', return_value=False), \
                patch.object(torch.backends.mps, 'is_available', return_value=True):
            self.assertEqual(compiler.resolve_device(None), torch.device('mps', 0))
            with self.assertRaises(ValueError):
                compiler.resolve_device(1)

        # Prefer CUDA and reject indices beyond the device count.
        with patch.object(torch.cuda, 'is_available', return_value=True), \
                patch.object(torch.cuda, 'device_count', return_value=2):
            self.assertEqual(compiler.resolve_device(None), torch.device('cuda', 0))
            self.assertEqual(compiler.resolve_device(1), torch.device('cuda', 1))
            with self.assertRaises(ValueError):
                compiler.resolve_device(2)

    def test_resolve_backend(self) -> None:
        '''Fall back to AOTI only for auto and use TensorRT when available; return None.'''
        cpu = torch.device('cpu')
        cuda = torch.device('cuda', 0)

        # Explicit AOTI never imports TensorRT, and auto falls back on unsupported devices.
        with patch.object(compiler, 'load_tensorrt') as load:
            self.assertEqual(compiler.resolve_backend('aoti', cuda), 'aoti')
            self.assertEqual(compiler.resolve_backend('auto', cpu), 'aoti')
            load.assert_not_called()
        with self.assertRaises(ValueError):
            compiler.resolve_backend('tensorrt', cpu)
        with self.assertRaises(ValueError):
            compiler.resolve_backend('unknown', cpu)

        # Require the Dynamo frontend, the C++ runtime, and the compile/save APIs.
        complete = Mock()
        complete.ENABLED_FEATURES = SimpleNamespace(
            dynamo_frontend=True, torch_tensorrt_runtime=True)
        incomplete = Mock()
        incomplete.ENABLED_FEATURES = SimpleNamespace(
            dynamo_frontend=True, torch_tensorrt_runtime=False)
        with patch.object(torch.version, 'hip', None), \
                patch.object(compiler.platform, 'system', return_value='Linux'):
            with patch.object(compiler, 'load_tensorrt', return_value=complete):
                self.assertEqual(compiler.resolve_backend('auto', cuda), 'tensorrt')
                self.assertEqual(compiler.resolve_backend('tensorrt', cuda), 'tensorrt')
            with patch.object(compiler, 'load_tensorrt', return_value=incomplete):
                self.assertEqual(compiler.resolve_backend('auto', cuda), 'aoti')
                with self.assertRaises(ImportError):
                    compiler.resolve_backend('tensorrt', cuda)

        # Distinguish ROCm from NVIDIA CUDA even though both use the cuda device name.
        with patch.object(torch.version, 'hip', '7'), \
                patch.object(compiler.platform, 'system', return_value='Linux'):
            self.assertEqual(compiler.resolve_backend('auto', cuda), 'aoti')

    def test_create_metadata(self) -> None:
        '''Record the actual conditions and the GPU runtime as strings; return None.'''
        source = {'model_name': 'probe', 'batch_size': 1}
        with patch.object(torch.version, 'hip', None):
            metadata = compiler.create_metadata(
                'tensorrt', torch.device('cuda', 0), torch.float16, 8, source)
        self.assertEqual(metadata, {
            'schema_version': '1', 'backend': 'tensorrt', 'device_type': 'cuda',
            'precision': 'fp16', 'input_dtype': 'float16', 'batch_size': '8',
            'gpu_runtime': 'cuda', 'model_name': 'probe',
        })
        metadata = compiler.create_metadata('aoti', torch.device('mps', 0), torch.float32, 1, {})
        self.assertEqual(metadata['gpu_runtime'], 'metal')
        metadata = compiler.create_metadata('aoti', torch.device('cpu'), torch.float32, 1, {})
        self.assertNotIn('gpu_runtime', metadata)

    def test_tensorrt_package(self) -> None:
        '''Forward the selected GPU and flags and save an AOTInductor package; return None.'''
        backend = Mock()
        backend.runtime.TorchTensorRTModule = type('TorchTensorRTModule', (), {})
        inputs = Mock()
        metadata = {'backend': 'tensorrt'}
        with patch.object(compiler, 'load_tensorrt', return_value=backend):
            compiler.save_tensorrt_package(
                Path('model.pt2'), Mock(), inputs, metadata, device=torch.device('cuda', 2),
                require_full_compilation=True, truncate_long_and_double=True)
        backend.Device.assert_called_once_with(gpu_id=2)
        kwargs = backend.dynamo.compile.call_args.kwargs
        self.assertEqual(kwargs['device'], backend.Device.return_value)
        self.assertEqual(kwargs['arg_inputs'], [inputs])
        self.assertTrue(kwargs['require_full_compilation'])
        self.assertTrue(kwargs['truncate_double'])
        self.assertFalse(kwargs['use_python_runtime'])
        save_kwargs = backend.save.call_args.kwargs
        self.assertEqual(save_kwargs['output_format'], 'aot_inductor')
        self.assertEqual(save_kwargs['inductor_configs'], {'aot_inductor.metadata': metadata})

    def test_save_files(self) -> None:
        '''Check input and output file handling and recorded conditions; return None.'''
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            source = directory / 'model.ep'
            output = directory / 'model.pt2'
            save_exported_probe(source, EchoModel(), 2)

            # Reject identical paths, missing inputs, and TensorRT options for AOTI.
            with self.assertRaises(ValueError):
                compiler.save_compiled_model(source, source, gpu=-1)
            with self.assertRaises(FileNotFoundError):
                compiler.save_compiled_model(directory / 'missing.ep', output, gpu=-1)
            with self.assertRaises(ValueError):
                compiler.save_compiled_model(
                    source, output, backend='aoti', gpu=-1, batch_size=2,
                    require_full_compilation=False)
            self.assertFalse(output.exists())

            # Compile with the stored batch and record the actual conditions in the package.
            compiler.save_compiled_model(source, output, backend='auto', gpu=-1, batch_size=2)
            metadata = read_model_metadata(output)
            self.assertEqual(
                {key: metadata[key] for key in (
                    'schema_version', 'backend', 'device_type', 'precision',
                    'input_dtype', 'batch_size')},
                {'schema_version': '1', 'backend': 'aoti', 'device_type': 'cpu',
                 'precision': 'fp32', 'input_dtype': 'float32', 'batch_size': '2'})
            self.assertNotIn('gpu_runtime', metadata)

            # Do not overwrite an existing package.
            with self.assertRaises(FileExistsError):
                compiler.save_compiled_model(source, output, gpu=-1, batch_size=2)


if __name__ == '__main__':
    unittest.main()
