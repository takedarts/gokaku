from pathlib import Path
from typing import List

import numpy as np

from .board import Board
from .config import (DEFAULT_BATCH_SIZE, DEFAULT_THREADS_PER_GPU,
                     MODEL_VALUE_OFFSET)
from .native import NativeInferenceModel, NativeInferenceProcessor
from .validation import validate_device_ids, validate_inference_sizes


class Processor(object):
    '''Manage model inference and batched board evaluation.'''
    def __init__(
        self,
        model: str | Path,
        gpus: List[int] = [-1],
        fp16: bool = False,
        deterministic: bool = False,
        batch_size: int = DEFAULT_BATCH_SIZE,
        threads_per_gpu: int = DEFAULT_THREADS_PER_GPU,
        cache_size: int = 0,
    ) -> None:
        '''Initialize the inference processor.
        Args:
            model (str | Path): Model file path.
            gpus (List[int]): Device IDs to use (CPU: -1).
            fp16 (bool): True to use half precision.
            deterministic (bool): True for reproducible results.
            batch_size (int): Inference batch size.
            threads_per_gpu (int): Inference threads per device.
            cache_size (int): Inference cache capacity.
        '''
        # Validate devices and size calculations before starting inference threads.
        validate_inference_sizes(len(gpus), batch_size, threads_per_gpu)
        validate_device_ids(gpus, NativeInferenceModel.get_available_gpus())

        if not Path(model).exists():
            raise FileNotFoundError(f'File not found: {model}')

        self.native = NativeInferenceProcessor(
            str(model), gpus, fp16, deterministic, batch_size, threads_per_gpu, cache_size)

    def evaluate(self, boards: List[Board]) -> np.ndarray:
        '''Evaluate multiple positions in one batch.
        Args:
            boards (List[Board]): Positions to evaluate.
        Returns:
            np.ndarray: Evaluation values from each position's side-to-move perspective.
        '''
        # Convert each position into the model input format.
        inputs = np.stack([board.get_inputs() for board in boards])

        # Run batch inference and convert win probabilities to values from -1 to 1.
        outputs = self.execute(inputs)
        return outputs[:, MODEL_VALUE_OFFSET] * 2.0 - 1.0

    def execute(self, inputs: np.ndarray) -> np.ndarray:
        '''Run inference.
        Args:
            inputs (np.ndarray): Input data.
        Returns:
            np.ndarray: Model outputs.
        '''
        return self.native.execute(inputs)

    def get_batch_fill_rate(self) -> float:
        '''Get the ratio of inference requests included in the batch.
        Returns:
            float: Ratio of inference requests included in the batch
        '''
        return self.native.get_batch_fill_rate()

    def get_cache_hit_rate(self) -> float:
        '''Get the cache hit rate of inference.
        Returns:
            float: Cache hit rate of inference
        '''
        return self.native.get_cache_hit_rate()
