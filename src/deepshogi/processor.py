from pathlib import Path
from typing import List

import numpy as np

from .board import Board
from .config import (DEFAULT_BATCH_SIZE, DEFAULT_THREADS_PER_GPU,
                     MODEL_VALUE_OFFSET)
from .native import NativeInferenceProcessor


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
            gpus (List[int]): GPU IDs to use.
            fp16 (bool): Whether to use FP16.
            deterministic (bool): True to request reproducible results.
            batch_size (int): Maximum batch size.
            threads_per_gpu (int): Number of threads per GPU.
            cache_size (int): Evaluation cache capacity.
        '''
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
