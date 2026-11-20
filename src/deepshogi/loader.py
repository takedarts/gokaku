import logging
from collections.abc import Generator
from contextlib import ExitStack, contextmanager
from pathlib import Path
from tempfile import TemporaryDirectory
from typing import Literal, Sequence
from zipfile import BadZipFile, ZipFile

import torch
from torch.export.pt2_archive.constants import AOTINDUCTOR_DIR, MODELS_FILENAME_FORMAT

from .config import DEFAULT_BATCH_SIZE, DEFAULT_THREADS_PER_GPU
from .gpu import get_default_gpus, read_model_metadata
from .native import NativeInferenceModel
from .processor import Processor
from .validation import validate_inference_sizes

LOGGER = logging.getLogger(__name__)


def _get_model_format(path: Path) -> Literal['ep', 'pt2']:
    '''Determine the model format from the file names inside the archive.

    Args:
        path (Path): Model path specified by the user.
    Returns:
        Literal['ep', 'pt2']: ExportedProgram or compiled package format.
    Raises:
        ValueError: If the archive is corrupted or is not a supported model format.
    '''
    message = f'Expected an ExportedProgram or a compiled PT2 package: {path}'
    try:
        # Match the internal files after removing the archive root name added by PyTorch.
        with ZipFile(path) as archive:
            entries = {name.replace('\\', '/') for name in archive.namelist()}
    except BadZipFile as error:
        raise ValueError(message) from error

    records = {name.partition('/')[2] for name in entries}

    # Treat archives containing compiled artifacts as PT2 even if a graph is also bundled.
    if any(name.startswith(AOTINDUCTOR_DIR) for name in records):
        return 'pt2'

    # Recognize the current EP layout and the older EP layout readable by the compiler.
    if (MODELS_FILENAME_FORMAT.format('model') in records
            or 'serialized_exported_program.json' in entries):
        return 'ep'

    raise ValueError(message)


def _get_default_ep_gpus() -> list[int]:
    '''Select all available GPUs that can share one PT2 when no EP device is specified.

    Returns:
        list[int]: GPU IDs of the same model as the first GPU, or [-1] for the CPU without GPUs.
    '''
    # Take the GPU IDs, excluding the CPU, in order from the same native list as the processor.
    gpus = [gpu for gpu in NativeInferenceModel.get_available_gpus() if gpu >= 0]

    # Compile and run inference on the CPU when no GPU is available.
    if not gpus:
        return [-1]

    # MPS has only GPU 0, so compare models only for multiple CUDA/ROCm GPUs.
    if len(gpus) == 1 or not torch.cuda.is_available():
        return gpus

    # Use only GPUs of the same model as the first so that they can share its compiled PT2.
    names = {gpu: torch.cuda.get_device_name(gpu) for gpu in gpus}
    selected = [gpu for gpu in gpus if names[gpu] == names[gpus[0]]]
    excluded = [gpu for gpu in gpus if gpu not in selected]

    # Log excluded GPUs as a warning so that users can notice them.
    if excluded:
        LOGGER.warning(
            'GPUs %s are excluded because their model differs from GPU %d (%s).',
            excluded, gpus[0], names[gpus[0]])

    return selected


def _get_ep_gpus(gpus: Sequence[int] | None) -> list[int]:
    '''Select devices that can compile an EP and run inference with the same package.

    Args:
        gpus (Sequence[int] | None): GPU IDs in the specified order. -1 is the CPU, None is auto.
    Returns:
        list[int]: Execution device IDs whose first element is the compilation target.
    Raises:
        ValueError: If a device is unavailable or the combination cannot share one PT2.
    '''
    from compile import resolve_device

    # Without a list, use all available GPUs of the same model as the first GPU.
    if gpus is None:
        return _get_default_ep_gpus()

    if not gpus:
        raise ValueError('Specify at least one device for the ExportedProgram.')

    # Check all execution devices before compiling and keep the specified order.
    devices = [resolve_device(gpu) for gpu in gpus]
    device = devices[0]

    if len(set(devices)) != len(devices) or any(d.type != device.type for d in devices):
        raise ValueError('Devices for a shared PT2 package must be distinct and of the same type.')

    if (len(devices) > 1 and device.type == 'cuda'
            and len({torch.cuda.get_device_name(d) for d in devices}) != 1):
        raise ValueError('GPUs must have the same model for a shared PT2 package.')

    return list(gpus)


@contextmanager
def open_processor(
    model: str | Path,
    *,
    gpus: Sequence[int] | None = None,
    threads_per_gpu: int = DEFAULT_THREADS_PER_GPU,
    cache_size: int = 0,
    backend: str = 'auto',
    fp16: bool = False,
    batch_size: int = DEFAULT_BATCH_SIZE,
) -> Generator[Processor, None, None]:
    '''Load the specified model and yield the processor after deleting the temporary PT2.

    An EP is compiled into a temporary PT2 on the first device and loaded onto all devices.
    backend, fp16, and batch_size are used only to compile an EP; a PT2 uses its stored conditions.

    Args:
        model (str | Path): Path to an EP or a compiled PT2.
        gpus (Sequence[int] | None): GPU IDs in the specified order. -1 is the CPU, None is auto.
        threads_per_gpu (int): Number of inference threads per device.
        cache_size (int): Capacity of the evaluation result cache.
        backend (str): auto, aoti, or tensorrt used to compile an EP.
        fp16 (bool): True to compile an EP with FP16.
        batch_size (int): Fixed batch size used to compile an EP.
    Yields:
        Processor: Processor configured with the execution conditions of the compiled model.
    '''
    path = Path(model)
    model_format = _get_model_format(path)

    with ExitStack() as stack:
        # Compile an EP on the first device with the specified precision and fixed batch.
        if model_format == 'ep':
            from compile import save_compiled_model

            gpus = _get_ep_gpus(gpus)

            # Validate the size calculations of the configuration before the slow compilation.
            validate_inference_sizes(len(gpus), batch_size, threads_per_gpu)

            directory = stack.enter_context(TemporaryDirectory(prefix='deepshogi_model_'))
            package = Path(directory) / 'model.pt2'
            save_compiled_model(
                path, package, backend=backend, gpu=gpus[0], fp16=fp16, batch_size=batch_size)
            path = package

        # Initialize the processor with the precision and batch size recorded in the PT2.
        metadata = read_model_metadata(path)
        device_ids, fp16 = get_default_gpus(gpus, model=metadata)

        processor = Processor(
            model=path,
            gpus=device_ids,
            fp16=fp16,
            batch_size=int(metadata['batch_size']),
            threads_per_gpu=threads_per_gpu,
            cache_size=cache_size)

    # Clean up the temporary directory after loading onto all devices, then yield the processor.
    yield processor
