import argparse
import importlib
import json
import logging
import platform
import sys
import tempfile
from contextlib import nullcontext
from copy import deepcopy
from pathlib import Path
from types import ModuleType
from typing import Any

import torch
import torch._inductor
from torch.export import ExportedProgram
from torch.export.passes import move_to_device_pass
from torch.fx import GraphModule

from deepshogi.config import DEFAULT_BATCH_SIZE, MODEL_INPUT_SIZE
from deepshogi.fileutil import move_file
from deepshogi.log import start_logging

LOGGER = logging.getLogger(__name__)


def parse_args() -> argparse.Namespace:
    '''Parse the ExportedProgram input/output paths and the PT2 conversion conditions.
    Returns:
        argparse.Namespace: Input/output paths and conversion conditions.
    '''
    parser = argparse.ArgumentParser(description='Compile an ExportedProgram to a PT2 package.')
    parser.add_argument('input', type=str, help='Path to the input ExportedProgram.')
    parser.add_argument('output', type=str, help='Path to the output PT2 package.')
    parser.add_argument(
        '--backend', choices=('auto', 'aoti', 'tensorrt'), default='auto',
        help='Compilation backend; auto selects TensorRT when available, otherwise AOTI '
             '(default: auto).')
    parser.add_argument(
        '--batch-size', type=int, default=DEFAULT_BATCH_SIZE,
        help=f'Fixed batch size (default: {DEFAULT_BATCH_SIZE}).')
    parser.add_argument(
        '--gpu', type=int, default=None,
        help='GPU index. Use -1 for CPU. The default order is CUDA/ROCm GPU 0, '
             'MPS GPU 0, then CPU.')
    parser.add_argument('--fp16', action='store_true', help='Use FP16 instead of FP32.')
    parser.add_argument(
        '--no-require-full-compilation', action='store_true',
        help='Allow AOTInductor fallback for operations unsupported by TensorRT.')
    parser.add_argument(
        '--truncate-long-and-double', action='store_true',
        help='TensorRT compatibility option: truncate float64 to float32; retain int64.')
    parser.add_argument('--verbose', action='store_true', help='Print debug information.')
    args = parser.parse_args()

    # Detect invalid device indices and fixed batch sizes before conversion.
    if args.gpu is not None and args.gpu < -1:
        parser.error('--gpu must be -1 or a nonnegative GPU index')

    if args.batch_size < 1:
        parser.error('--batch-size must be positive')

    if (args.backend == 'aoti'
            and (args.no_require_full_compilation or args.truncate_long_and_double)):
        parser.error('TensorRT compilation options cannot be used with --backend aoti')

    return args


def resolve_device(gpu: int | None) -> torch.device:
    '''Select an available device in the order CUDA/ROCm, MPS, and CPU when unspecified.
    Args:
        gpu (int | None): -1 for the CPU, a GPU index, or None for automatic selection.
    Returns:
        torch.device: Available CPU, CUDA/ROCm, or MPS device.
    Raises:
        ValueError: If the specified GPU index is invalid or unavailable.
    '''
    # When unspecified, select index 0 of an available GPU, or the CPU if no GPU exists.
    if gpu is None:
        gpu_available = torch.cuda.is_available() or torch.backends.mps.is_available()
        gpu = 0 if gpu_available else -1

    if gpu == -1:
        return torch.device('cpu')

    if gpu < -1:
        raise ValueError('GPU index must be -1 or nonnegative')

    # CUDA and ROCm use the same PyTorch device name.
    if torch.cuda.is_available():
        if gpu >= torch.cuda.device_count():
            raise ValueError(f'GPU index is not available: {gpu}')

        return torch.device('cuda', gpu)

    # Specify the MPS index explicitly so that device checks in the graph match real tensors.
    if gpu == 0 and torch.backends.mps.is_available():
        return torch.device('mps', 0)

    raise ValueError(f'GPU index is not available: {gpu}')


def get_example_input(exported: ExportedProgram) -> torch.Tensor:
    '''Get the single tensor input of the board model from the ExportedProgram.
    Args:
        exported (ExportedProgram): Model with unpacked board inputs.
    Returns:
        torch.Tensor: Input tensor with a batch dimension and MODEL_INPUT_SIZE features.
    Raises:
        ValueError: If the input is not a single tensor or its shape differs.
    '''
    # Ensure the example inputs required for compilation are stored in the model.
    if exported.example_inputs is None:
        raise ValueError('The ExportedProgram has no example inputs')

    # Split the stored example inputs into positional and keyword arguments.
    args, kwargs = exported.example_inputs

    # Limit the board model input to a single tensor without keyword arguments.
    if len(args) != 1 or kwargs or not isinstance(args[0], torch.Tensor):
        raise ValueError('Expected one positional tensor input and no keyword inputs')

    # Get the board input tensor from the validated positional arguments.
    inputs = args[0]

    # Check that the input is 2D, the batch is not empty, and the features match the model.
    if inputs.ndim != 2 or inputs.shape[0] < 1 or inputs.shape[1] != MODEL_INPUT_SIZE:
        raise ValueError(f'Expected input shape (batch_size, {MODEL_INPUT_SIZE})')

    # Limit the floating-point precision handled by the conversion to FP32 and FP16.
    if inputs.dtype not in (torch.float32, torch.float16):
        raise ValueError('Only FP32 and FP16 inputs are supported')

    # Return the example input that satisfies all conditions.
    return inputs


def load_exported_model(path: str | Path) -> tuple[ExportedProgram, dict[str, Any]]:
    '''Load the ExportedProgram and the metadata stored with it.
    Args:
        path (str | Path): Path to the input model.
    Returns:
        tuple[ExportedProgram, dict[str, Any]]: Input model and additional metadata.
    Raises:
        ValueError: If the metadata is invalid or differs from the actual input conditions.
    '''
    # Load through a file object even when the extension is .ep.
    extras = {'metadata.json': ''}

    with Path(path).open('rb') as stream:
        exported = torch.export.load(stream, extra_files=extras)

    metadata = json.loads(extras['metadata.json']) if extras['metadata.json'] else {}

    if not isinstance(metadata, dict):
        raise ValueError('metadata.json must contain a JSON object')

    # Also check the input record so that stale conditions are not carried to the output.
    inputs = get_example_input(exported)
    conditions = {
        'device_type': inputs.device.type,
        'input_dtype': str(inputs.dtype).removeprefix('torch.'),
        'precision': 'fp16' if inputs.dtype == torch.float16 else 'fp32',
        'batch_size': str(inputs.shape[0]),
    }

    for key, value in conditions.items():
        if key in metadata and str(metadata[key]) != value:
            raise ValueError(f'Input metadata does not match the model: {key}')

    return exported, metadata


def has_dynamic_batch(exported: ExportedProgram) -> bool:
    '''Determine whether the batch dimension of the board input was exported as dynamic.
    Args:
        exported (ExportedProgram): Model with a single board input.
    Returns:
        bool: True if the batch dimension is dynamic and the feature dimension is fixed.
    Raises:
        ValueError: If the input shape information cannot be obtained.
    '''
    # Get the shape recorded at export from the placeholder of the user input.
    input_names = exported.graph_signature.user_inputs

    if len(input_names) != 1:
        raise ValueError('Expected one user input')

    nodes = [n for n in exported.graph.nodes if n.op == 'placeholder' and n.name == input_names[0]]
    value = nodes[0].meta.get('val') if nodes else None

    if not isinstance(value, torch.Tensor) or value.ndim != 2:
        raise ValueError('The ExportedProgram has no input shape information')

    # Treat models with a dynamic feature dimension as outside the board model specification.
    if isinstance(value.shape[1], torch.SymInt):
        raise ValueError('Only the batch dimension can be dynamic')

    return isinstance(value.shape[0], torch.SymInt)


def convert_graph_dtype(
    module: GraphModule, source: torch.dtype, target: torch.dtype,
) -> GraphModule:
    '''Replace the floating-point type fixed in the computation graph with the specified type.
    Args:
        module (GraphModule): Model obtained with ExportedProgram.module().
        source (torch.dtype): Input precision at export.
        target (torch.dtype): Input precision after conversion.
    Returns:
        GraphModule: Model with type conversions and type checks rewritten.
    '''
    # Remove nodes checking the type at export because they always fail after conversion.
    for node in list(module.graph.nodes):
        if node.target == torch.ops.aten._assert_tensor_metadata.default:
            module.graph.erase_node(node)

        # Replace types recorded as conversions to the input type with the converted type.
        elif node.target == torch.ops.aten.to.dtype and node.args[1] == source:
            node.args = (node.args[0], target, *node.args[2:])

        elif node.kwargs.get('dtype') == source:
            node.kwargs = {**node.kwargs, 'dtype': target}

    # Check the consistency of the rewritten graph and regenerate the executable code.
    module.graph.lint()
    module.recompile()

    return module


def remove_noop_slices(module: GraphModule) -> GraphModule:
    '''Remove slices that only take a whole dimension, which remain after a dynamic-batch export.
    Args:
        module (GraphModule): Model obtained with ExportedProgram.module().
    Returns:
        GraphModule: Model with unnecessary slices removed.
    '''
    # A slice with start 0, the maximum end, and step 1 returns its input, so use the input.
    for node in list(module.graph.nodes):
        if node.target != torch.ops.aten.slice.Tensor:
            continue

        args = list(node.args) + [None] * (5 - len(node.args))
        start, end, step = args[2:5]

        if start in (None, 0) and end in (None, sys.maxsize) and step in (None, 1):
            node.replace_all_uses_with(node.args[0])
            module.graph.erase_node(node)

    # Check the consistency of the rewritten graph and regenerate the executable code.
    module.graph.lint()
    module.recompile()

    return module


def prepare_exported_model(
    exported: ExportedProgram,
    *,
    batch_size: int,
    device: torch.device,
    dtype: torch.dtype,
) -> tuple[ExportedProgram, torch.Tensor]:
    '''Create the compilation input for the specified precision, device, and fixed batch.
    Args:
        exported (ExportedProgram): Original board model.
        batch_size (int): Fixed batch size of at least one.
        device (torch.device): Target device for compilation and execution.
        dtype (torch.dtype): FP32 or FP16.
    Returns:
        tuple[ExportedProgram, torch.Tensor]: Model prepared for the fixed batch and example input.
    Raises:
        ValueError: If a static-batch model would change its precision or batch, or the
            specified conditions are invalid.
    '''
    # Check the fixed batch and the input/output precision.
    if batch_size < 1 or dtype not in (torch.float32, torch.float16):
        raise ValueError('Expected a positive batch size and FP32 or FP16 dtype')

    example = get_example_input(exported)

    # Use a static-batch model only by moving the device when the precision and batch match.
    if not has_dynamic_batch(exported):
        if example.dtype != dtype or example.shape[0] != batch_size:
            raise ValueError(
                'A static-batch ExportedProgram cannot change its precision or batch size; '
                'recreate it with a dynamic batch dimension')

        prepared = move_to_device_pass(deepcopy(exported), device)
        return prepared, get_example_input(prepared)

    # Move the program including device specifications in the graph and get a runnable module.
    module = move_to_device_pass(deepcopy(exported), device).module()

    # Remove slices not generated by a fixed-batch export to obtain the same computation graph.
    module = remove_noop_slices(module)

    # When changing the precision, replace types fixed in the graph before converting weights.
    if example.dtype != dtype:
        module = convert_graph_dtype(module, example.dtype, dtype)

    module.to(device=device, dtype=dtype)

    # Re-export without dynamic dimensions and pass a fixed-batch graph to the compiler.
    inputs = example[:1].to(device=device, dtype=dtype).repeat(batch_size, 1).contiguous()
    prepared = torch.export.export(module, (inputs,), strict=False)

    return prepared, inputs


def create_metadata(
    backend: str, device: torch.device, dtype: torch.dtype,
    batch_size: int, source_metadata: dict[str, Any],
) -> dict[str, str]:
    '''Create string metadata readable by C++ from the actual conversion conditions.
    Args:
        backend (str): aoti or tensorrt.
        device (torch.device): Actual target device.
        dtype (torch.dtype): Actual input precision.
        batch_size (int): Actual fixed batch size.
        source_metadata (dict[str, Any]): Model name and other values stored in the input.
    Returns:
        dict[str, str]: Metadata embedded in the PT2 package.
    '''
    # Set the required conditions from the compile target instead of reusing input metadata.
    metadata = {
        'schema_version': '1',
        'backend': backend,
        'device_type': device.type,
        'precision': 'fp16' if dtype == torch.float16 else 'fp32',
        'input_dtype': str(dtype).removeprefix('torch.'),
        'batch_size': str(batch_size),
    }

    # CUDA and ROCm use the same device name, so distinguish the runtime by HIP availability.
    if device.type == 'cuda':
        metadata['gpu_runtime'] = 'rocm' if torch.version.hip is not None else 'cuda'

    # Record that MPS uses Metal; do not add a GPU runtime entry for the CPU.
    elif device.type == 'mps':
        metadata['gpu_runtime'] = 'metal'

    # Get the model structure name, which is carried over after conversion, from the input.
    model_name = source_metadata.get('model_name')

    # Add it to the output metadata only when a valid model name is stored.
    if isinstance(model_name, str) and model_name:
        metadata['model_name'] = model_name

    # Return metadata for the package consisting of string keys and values.
    return metadata


def load_tensorrt() -> ModuleType:
    '''Import Torch-TensorRT when checking TensorRT availability or converting.
    Returns:
        ModuleType: Imported torch_tensorrt module.
    Raises:
        ImportError: If the module or its shared libraries are unavailable.
    '''
    # Do not require Torch-TensorRT in environments that use only AOTI.
    try:
        return importlib.import_module('torch_tensorrt')
    except Exception as error:
        # Also handle Torch-TensorRT wrapping dependency load failures in a plain Exception.
        raise ImportError(f'torch_tensorrt is not available: {error}') from error


def resolve_backend(backend: str, device: torch.device) -> str:
    '''Determine the actual backend from the selected device and TensorRT availability.
    Args:
        backend (str): auto, aoti, or tensorrt.
        device (torch.device): Selected device used for conversion.
    Returns:
        str: aoti or tensorrt that is actually used.
    Raises:
        ValueError: If the backend name is invalid or the TensorRT conditions are unsupported.
        ImportError: If required features of an explicitly specified TensorRT are unavailable.
    '''
    # With an explicit AOTI, do not check or import TensorRT.
    if backend == 'aoti':
        return 'aoti'

    # Limit other values to automatic selection and explicit TensorRT.
    if backend not in ('auto', 'tensorrt'):
        raise ValueError('Expected backend auto/aoti/tensorrt')

    # Even with automatic selection, keep the device and check whether TensorRT can create a PT2.
    try:
        # ROCm also uses the cuda device name, so distinguish it from NVIDIA CUDA by HIP info.
        if device.type != 'cuda' or torch.version.hip is not None:
            raise ValueError('TensorRT requires an NVIDIA CUDA GPU')

        # Saving AOTInductor PT2 files with Torch-TensorRT requires Linux.
        if platform.system() != 'Linux':
            raise ValueError('TensorRT PT2 packaging requires Linux')

        # Import only when the device and OS are supported, and get the enabled features.
        tensorrt = load_tensorrt()
        features = getattr(tensorrt, 'ENABLED_FEATURES', None)

        # Check that the Dynamo frontend required to convert an ExportedProgram is enabled.
        # Do not treat missing feature information as available.
        if not getattr(features, 'dynamo_frontend', False):
            raise ImportError('Torch-TensorRT Dynamo frontend is not available')

        # Inference runs from LibTorch, so require the Python-independent C++ runtime.
        if not getattr(features, 'torch_tensorrt_runtime', False):
            raise ImportError('Torch-TensorRT C++ runtime is not available')

        # Besides the feature flags, also check the compile and save APIs actually called.
        if (not callable(getattr(getattr(tensorrt, 'dynamo', None), 'compile', None))
                or not callable(getattr(tensorrt, 'save', None))):
            raise ImportError('Torch-TensorRT Dynamo compile/save APIs are not available')
    except (ImportError, ValueError) as error:
        # Fall back by environment only for auto; do not cover model compilation failures.
        if backend != 'auto':
            raise

        # Record why TensorRT is unavailable and select AOTI on the same device.
        LOGGER.info('Backend auto: selected aoti on %s; TensorRT unavailable: %s', device, error)
        return 'aoti'

    # Prefer TensorRT in NVIDIA CUDA environments where the required features are available.
    if backend == 'auto':
        LOGGER.info('Backend auto: selected tensorrt on %s', device)

    # For both auto and explicit selection, return the backend confirmed to be available.
    return 'tensorrt'


def save_aoti_model(
    path: Path, exported: ExportedProgram, metadata: dict[str, str],
) -> None:
    '''Compile with AOTInductor and save a PT2 package with metadata.
    Args:
        path (Path): Destination of the temporary PT2 file.
        exported (ExportedProgram): Model prepared for the target conditions.
        metadata (dict[str, str]): Actual conversion conditions.
    '''
    # Store the metadata where the native package loader can read it.
    torch._inductor.aoti_compile_and_package(
        exported, package_path=str(path),
        inductor_configs={'aot_inductor.metadata': metadata.copy()})


def save_tensorrt_package(
    path: Path,
    exported: ExportedProgram,
    inputs: torch.Tensor,
    metadata: dict[str, str],
    *,
    device: torch.device,
    require_full_compilation: bool,
    truncate_long_and_double: bool,
) -> None:
    '''Convert to TensorRT with Dynamo and save it in an AOTInductor PT2 package.
    Args:
        path (Path): Destination of the temporary PT2 file.
        exported (ExportedProgram): Model prepared for the target conditions.
        inputs (torch.Tensor): Example input of the fixed batch.
        metadata (dict[str, str]): Actual conversion conditions.
        device (torch.device): Target NVIDIA CUDA device.
        require_full_compilation (bool): True to require TensorRT conversion of all operations.
        truncate_long_and_double (bool): True to convert float64 to float32.
    '''
    # Specify types through the model and inputs; do not combine explicit typing with
    # enabled_precisions.
    tensorrt = load_tensorrt()
    trt_model = tensorrt.dynamo.compile(
        exported, arg_inputs=[inputs], device=tensorrt.Device(gpu_id=device.index),
        use_explicit_typing=True, use_python_runtime=False, workspace_size=1 << 30,
        require_full_compilation=require_full_compilation,
        truncate_double=truncate_long_and_double)

    # If a TensorRT engine is returned directly, convert it to a savable ExportedProgram.
    if isinstance(trt_model, tensorrt.runtime.TorchTensorRTModule):
        trt_exported = torch.export.export(trt_model, (inputs,), strict=False)
        save_aoti_model(path, trt_exported, metadata)
        return

    # Store the TensorRT engine and fallback operations in the same PT2 format.
    tensorrt.save(
        trt_model, str(path), arg_inputs=[inputs],
        output_format='aot_inductor', retrace=True,
        inductor_configs={'aot_inductor.metadata': metadata.copy()})


def save_compiled_model(
    input_path: str | Path,
    output_path: str | Path,
    *,
    backend: str = 'auto',
    gpu: int | None = None,
    fp16: bool = False,
    batch_size: int = DEFAULT_BATCH_SIZE,
    require_full_compilation: bool = True,
    truncate_long_and_double: bool = False,
) -> None:
    '''Compile the input EP under the specified conditions and save it without overwriting.
    Args:
        input_path (str | Path): Path to the input ExportedProgram.
        output_path (str | Path): Path to the output PT2 package.
        backend (str): auto for automatic selection, aoti, or tensorrt.
        gpu (int | None): -1 for the CPU, a GPU index, or None for automatic selection.
        fp16 (bool): True to compile with FP16.
        batch_size (int): Fixed batch size of at least one.
        require_full_compilation (bool): True to require full conversion with TensorRT.
        truncate_long_and_double (bool): True to convert float64 to float32 with TensorRT.
    Raises:
        ValueError: If the backend, device, or conversion conditions are unsupported.
        FileExistsError: If the output file already exists.
        FileNotFoundError: If the input file or output directory does not exist.
    '''
    # Check files and arguments without changing the input or an existing output.
    source_path = Path(input_path)
    destination_path = Path(output_path)

    if source_path.resolve() == destination_path.resolve():
        raise ValueError('Input and output paths must differ')
    elif destination_path.exists() or destination_path.is_symlink():
        raise FileExistsError(destination_path)
    elif not source_path.is_file():
        raise FileNotFoundError(source_path)
    elif not destination_path.parent.is_dir():
        raise FileNotFoundError(destination_path.parent)
    elif backend not in ('auto', 'aoti', 'tensorrt') or batch_size < 1:
        raise ValueError('Expected backend auto/aoti/tensorrt and a positive batch size')

    # Determine the device first, then select a backend available in that environment.
    device = resolve_device(gpu)
    resolved_backend = resolve_backend(backend, device)

    # Even when auto selects AOTI, do not silently ignore TensorRT-specific options.
    if resolved_backend == 'aoti' and (not require_full_compilation or truncate_long_and_double):
        raise ValueError('TensorRT compilation options cannot be used with backend aoti')

    # Determine the data type used for compilation.
    dtype = torch.float16 if fp16 else torch.float32

    # Log the compilation settings.
    LOGGER.info('Compile: backend=%s, device=%s, dtype=%s, batch=%d',
                resolved_backend, device, dtype, batch_size)

    # Apply the GPU selection to input preparation, building, and saving.
    device_context: torch.cuda.device | nullcontext[None] = (
        torch.cuda.device(device) if device.type == 'cuda' else nullcontext())

    with device_context, torch.no_grad():
        exported, source_metadata = load_exported_model(source_path)
        prepared, inputs = prepare_exported_model(
            exported, batch_size=batch_size, device=device, dtype=dtype)
        metadata = create_metadata(resolved_backend, device, dtype, batch_size, source_metadata)

        # Create the .pt2 in the OS temporary directory so that no work files remain in the
        # data directory.
        with tempfile.TemporaryDirectory(prefix='compile_') as directory:
            staged_path = Path(directory) / 'model.pt2'

            if resolved_backend == 'aoti':
                save_aoti_model(staged_path, prepared, metadata)
            else:
                save_tensorrt_package(
                    staged_path, prepared, inputs, metadata, device=device,
                    require_full_compilation=require_full_compilation,
                    truncate_long_and_double=truncate_long_and_double)

            # On failure discard only the temporary file, and publish only a complete package.
            if not staged_path.is_file():
                raise RuntimeError('The compiler did not create a PT2 package')

            move_file(staged_path, destination_path, overwrite=False)


def main() -> None:
    '''Convert the specified file and propagate exceptions raised during conversion.'''
    # Parse the command-line arguments.
    args = parse_args()

    # Configure logging.
    start_logging(debug=args.verbose)

    # Convert and save the model under the specified conditions.
    save_compiled_model(
        args.input, args.output,
        backend=args.backend,
        gpu=args.gpu,
        fp16=args.fp16,
        batch_size=args.batch_size,
        require_full_compilation=not args.no_require_full_compilation,
        truncate_long_and_double=args.truncate_long_and_double)

    # Log the path of the successfully saved file.
    LOGGER.info('saved: %s', args.output)


if __name__ == '__main__':
    main()
