from typing import List, Sequence, Tuple

from .native import NativeInferenceModel
from .validation import validate_device_ids


def get_default_gpus(
    gpus: Sequence[int] | None,
    fp16: bool,
) -> Tuple[List[int], bool]:
    '''Return GPU IDs and FP16 availability appropriate for the environment.
    If gpus is None, return the IDs of available GPUs.
    Otherwise, use the specified GPU IDs.
    Disable FP16 if gpus contains -1.
    Warn about invalid GPU IDs and ignore them.
    Args:
        gpus (Sequence[int] | None): Requested GPU IDs.
        fp16 (bool): True to use FP16 computation.
    Returns:
        Tuple[List[int], bool]: GPU IDs and whether to use FP16.
    '''
    # Get list of available GPU IDs
    available_gpus = NativeInferenceModel.get_available_gpus()

    # If GPU ID list is not specified, set the list of available GPU IDs
    if gpus is None:
        # If GPU IDs are available, exclude CPU
        if max(available_gpus, default=-1) >= 0:
            new_gpus = [gpu for gpu in available_gpus if gpu >= 0]
        else:
            new_gpus = available_gpus
    # Keep the requested configuration and reject unavailable devices.
    else:
        new_gpus = list(gpus)

    validate_device_ids(new_gpus, available_gpus)

    # If -1 is included in the GPU ID list, disable FP16 usage
    if -1 in new_gpus:
        new_fp16 = False
    # Otherwise, use the specified FP16 setting
    else:
        new_fp16 = fp16

    return new_gpus, new_fp16
