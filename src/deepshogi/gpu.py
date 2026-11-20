import argparse
from pathlib import Path
from typing import Mapping, Sequence

from .native import NativeInferenceModel
from .validation import INT32_MAX, validate_device_ids


def parse_gpu_ids(value: str) -> list[int]:
    '''Parse comma-separated device IDs.
    Args:
        value (str): List of GPU IDs (-1 for the CPU).
    Returns:
        list[int]: List of device IDs.
    Raises:
        argparse.ArgumentTypeError: If an ID is invalid or duplicated.
    '''
    # Convert each element to an integer device ID while keeping the specified order
    try:
        gpus = [int(item) for item in value.split(',')]
    except ValueError as error:
        raise argparse.ArgumentTypeError('GPU IDs must be comma-separated integers.') from error

    if any(gpu < -1 or gpu > INT32_MAX for gpu in gpus):
        raise argparse.ArgumentTypeError('GPU IDs must be -1 (CPU) or non-negative integers.')

    if len(set(gpus)) != len(gpus):
        raise argparse.ArgumentTypeError('GPU IDs must not contain duplicates.')

    return gpus


def read_model_metadata(model: str | Path) -> dict[str, str]:
    '''Read the execution conditions from a compiled PT2 before loading the model itself.
    Args:
        model (str | Path): Path to a model created by compile.py.
    Returns:
        dict[str, str]: Metadata stored in the model entry of the PT2.
    Raises:
        ValueError: If the file is not a PT2 model created by compile.py.
    '''
    # Skip importing PyTorch for existing calls that do not specify a model.
    import torch

    # Check the package regardless of its extension with the same loader as the native module.
    read_metadata = getattr(torch._C._aoti.AOTIModelPackageLoader, 'load_metadata_from_package')
    message = f'Expected a PT2 model created by compile.py: {model}'

    # If reading the model entry fails, report the cause as a ValueError containing the path.
    try:
        metadata: dict[str, str] = read_metadata(str(model), 'model')
    except (OSError, RuntimeError, ValueError) as error:
        raise ValueError(message) from error

    # Check the format written by compile.py and the fixed batch size.
    if metadata.get('schema_version') != '1' or metadata.get('backend') not in ('aoti', 'tensorrt'):
        raise ValueError(message)

    # The fixed batch size must be an integer of at least one written in ASCII digits.
    batch_text = metadata.get('batch_size', '')

    if not batch_text.isascii() or not batch_text.isdigit() or int(batch_text) < 1:
        raise ValueError(f'Invalid PT2 batch_size: {batch_text!r}')

    return metadata


def get_default_gpus(
    gpus: Sequence[int] | None,
    fp16: bool = False,
    model: str | Path | Mapping[str, str] | None = None,
) -> tuple[list[int], bool]:
    '''Select device IDs and precision from the environment and the model's stored conditions.
    Args:
        gpus (Sequence[int] | None): IDs to use. None selects compatible devices automatically.
        fp16 (bool): Whether to use FP16 on the GPU when no model is specified.
        model (str | Path | Mapping[str, str] | None): PT2 path or metadata already read.
    Returns:
        tuple[list[int], bool]: List of device IDs and whether to use FP16.
    Raises:
        ValueError: If a specified device is unavailable or the stored conditions are invalid.
    '''
    # Get the IDs selectable on the native side. The CPU ID is -1.
    available_gpus = NativeInferenceModel.get_available_gpus()

    # Without a model, prefer GPUs, and disable FP16 for explicit lists that include the CPU.
    if model is None:
        gpu_ids = [gpu for gpu in available_gpus if gpu >= 0]
        selected = list(gpus) if gpus is not None else (gpu_ids or available_gpus)
        validate_device_ids(selected, available_gpus)

        return selected, fp16 and -1 not in selected

    # Use the precision fixed in the PT2. FP16 models for the CPU are also handled here.
    # Open the PT2 only when a path is given, and use metadata already read as is.
    metadata = read_model_metadata(model) if isinstance(model, (str, Path)) else model
    precision = metadata.get('precision')

    if precision not in ('fp32', 'fp16'):
        raise ValueError(f'Invalid PT2 precision: {precision}')

    device_type = metadata.get('device_type')
    runtime = metadata.get('gpu_runtime')

    # For a CPU model without a GPU runtime, use only the CPU as a candidate.
    if device_type == 'cpu' and runtime is None:
        compatible = [gpu for gpu in available_gpus if gpu == -1]
    else:
        import torch

        # Determine the GPU type in the same order as C++, distinguishing CUDA and ROCm,
        # which share a device name.
        if torch.cuda.is_available():
            platform = ('cuda', 'rocm' if torch.version.hip is not None else 'cuda')
        elif torch.backends.mps.is_available():
            platform = ('mps', 'metal')
        else:
            raise ValueError(f'No GPU is available for PT2 device {device_type}/{runtime}.')

        # Ensure both the PT2 device type and GPU runtime match the execution environment.
        if (device_type, runtime) != platform:
            raise ValueError(f'PT2 device {device_type}/{runtime} is unavailable on this system.')

        # A GPU PT2 cannot run on the CPU, so exclude the CPU ID from the candidates.
        compatible = [gpu for gpu in available_gpus if gpu >= 0]

    # Keep the order of an explicit list and reject the whole list if any ID is unavailable.
    selected = list(gpus) if gpus is not None else compatible

    validate_device_ids(selected, compatible)

    return selected, precision == 'fp16'
