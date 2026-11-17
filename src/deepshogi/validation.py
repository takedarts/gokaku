import math
import struct
from typing import Sequence

from .config import MODEL_INPUT_PACK_SIZE, MODEL_OUTPUT_PACK_SIZE, MODEL_OUTPUT_SIZE

INT32_MAX = (1 << 31) - 1


def validate_positive_int(name: str, value: int) -> None:
    '''Validate that a setting is a positive 32-bit integer.
    Args:
        name (str): Setting name.
        value (int): Setting value (integers are also accepted for floats).
    Raises:
        ValueError: If the value is not an integer or is outside the supported range.
    '''
    # Prevent implicit conversion of booleans and fractional values.
    if isinstance(value, bool) or not isinstance(value, int) or not 1 <= value <= INT32_MAX:
        raise ValueError(f'{name} must be an integer between 1 and {INT32_MAX}.')


def validate_positive_float(name: str, value: float) -> None:
    '''Validate that a setting remains finite and positive as a 32-bit float.
    Args:
        name (str): Setting name.
        value (float): Setting value (integers are also accepted for floats).
    Raises:
        ValueError: If the value is not numeric or does not convert to a finite positive float.
    '''
    message = f'{name} must be finite and greater than zero as a 32-bit float.'

    # Reject implicit conversions of booleans and nonnumeric values.
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ValueError(message)

    # Detect overflow to infinity and underflow to zero during C++ conversion.
    try:
        native_value = struct.unpack('f', struct.pack('f', value))[0]
    except (OverflowError, struct.error):
        raise ValueError(message) from None
    if not math.isfinite(native_value) or native_value <= 0.0:
        raise ValueError(message)


def validate_device_ids(gpus: Sequence[int], available_gpus: Sequence[int]) -> None:
    '''Validate that devices are specified and all are available.
    Args:
        gpus (Sequence[int]): Device IDs to use (CPU: -1).
        available_gpus (Sequence[int]): Available execution device IDs.
    Raises:
        ValueError: If no devices are specified or a device ID is unavailable.
    '''
    # Reject an invalid configuration instead of silently removing individual devices.
    if not gpus:
        raise ValueError('gpus must contain at least one available device.')
    for gpu in gpus:
        if isinstance(gpu, bool) or not isinstance(gpu, int) or gpu not in available_gpus:
            raise ValueError(f'Unavailable device ID: {gpu}.')


def validate_inference_sizes(device_count: int, batch_size: int, threads_per_gpu: int) -> None:
    '''Validate inference settings and native size calculation ranges.
    Args:
        device_count (int): Number of execution devices.
        batch_size (int): Inference batch size.
        threads_per_gpu (int): Inference threads per device.
    Raises:
        ValueError: If a setting or a calculated size is outside the supported range.
    '''
    # Require at least one thread and one batch item.
    validate_positive_int('device_count', device_count)
    validate_positive_int('batch_size', batch_size)
    validate_positive_int('threads_per_gpu', threads_per_gpu)

    # Keep inference thread counts and evaluation queue sizes within 32-bit range.
    thread_size = device_count * threads_per_gpu
    if thread_size > INT32_MAX or batch_size * thread_size * 10 > INT32_MAX:
        raise ValueError('Inference thread count or evaluation queue size exceeds int32 range.')

    # Keep batch buffer element counts representable by native integer arithmetic.
    buffer_width = max(MODEL_INPUT_PACK_SIZE, MODEL_OUTPUT_PACK_SIZE, MODEL_OUTPUT_SIZE)
    if batch_size * buffer_width > INT32_MAX:
        raise ValueError('batch_size is too large for inference buffers.')
