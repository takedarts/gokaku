from typing import List

from libc.stdint cimport int32_t
from libcpp.vector cimport vector

import numpy
cimport numpy

from deepshogi.config import MODEL_INPUT_PACK_SIZE, MODEL_OUTPUT_PACK_SIZE, MODEL_OUTPUT_SIZE

from pyx.inference cimport InferenceModel, InferenceProcessor


cdef class NativeInferenceModel:
    cdef InferenceModel *model

    @staticmethod
    def get_available_gpus() -> List[int]:
        '''Returns the list of available GPU indices.
        Returns:
            list[int]: List of GPU indices
        '''
        return InferenceModel.getAvailableGPUs()

    def __cinit__(self, model: str, gpu: int, fp16: bool, deterministic: bool) -> None:
        '''Creates a model object.
        Args:
            model (str): Path to the model file
            gpu (int): ID of the GPU to use
            fp16 (bool): True to compute with FP16
            deterministic (bool): True to make computation results reproducible
        '''
        self.model = new InferenceModel(model.encode('utf-8'), gpu, fp16, deterministic)

    def __dealloc__(self) -> None:
        del self.model

    def forward(self, inputs: numpy.ndarray, masks: numpy.ndarray) -> numpy.ndarray:
        '''Runs inference.
        Args:
            inputs (numpy.ndarray): Input data
            masks (numpy.ndarray): Output data mask
        Returns:
            numpy.ndarray: Output data
        '''
        cdef numpy.ndarray[numpy.float32_t, ndim=2, mode="c"] outputs = numpy.zeros(
            (inputs.shape[0], MODEL_OUTPUT_SIZE), dtype=numpy.float32)

        cdef int32_t size = <int32_t>inputs.shape[0]
        cdef int32_t* in_data = <int32_t*> inputs.data
        cdef int32_t* mask_data = <int32_t*> masks.data
        cdef float* out_data = <float*> outputs.data

        self.model.forward(in_data, mask_data, out_data, size)

        return outputs

    def is_cuda(self) -> bool:
        '''Returns whether CUDA is being used.
        Returns:
            bool: True if CUDA is being used
        '''
        return self.model.isCuda()


cdef class NativeInferenceProcessor:
    cdef InferenceProcessor *processor

    def __cinit__(
        self, model: str,
        gpus: List[int],
        fp16: bool,
        deterministic: bool,
        batch_size: int,
        threads_per_gpu: int,
        cache_size: int,
    ) -> None:
        '''Creates an inference processor object.
        Args:
            model (str): Path to the model file
            gpus (list[int]): List of GPU IDs to use
            fp16 (bool): True to compute with FP16
            deterministic (bool): True to make computation results reproducible
            batch_size (int): Batch size for inference
            threads_per_gpu (int): Number of threads per GPU
            cache_size (int): Cache size for inference results
        '''
        cdef vector[int32_t] gpu_vector
        for gpu in gpus:
            gpu_vector.push_back(gpu)

        self.processor = new InferenceProcessor(
            model.encode('utf-8'), gpu_vector, fp16, deterministic,
            batch_size, threads_per_gpu, cache_size)

    def __dealloc__(self) -> None:
        del self.processor

    def predict(self, board: NativeBoard) -> float:# type: ignore
        '''Runs inference on the given board.
        Args:
            board (NativeBoard): Board object
        Returns:
            float: Evaluation value
        '''
        return self.processor.predict(board.board)

    def execute(self, inputs: numpy.ndarray) -> numpy.ndarray:
        '''Runs inference.
        Args:
            inputs (numpy.ndarray): Input data
        Returns:
            numpy.ndarray: Output data
        '''
        cdef numpy.ndarray[numpy.float32_t, ndim=2, mode="c"] outputs = numpy.empty(
            (inputs.shape[0], MODEL_OUTPUT_SIZE), dtype=numpy.float32)
        cdef numpy.ndarray[numpy.int32_t, ndim=2, mode="c"] masks = numpy.full(
            (inputs.shape[0], MODEL_OUTPUT_PACK_SIZE), -1, dtype=numpy.int32)
        cdef int32_t size = <int32_t>inputs.shape[0]
        cdef int32_t* in_data = <int32_t*> inputs.data
        cdef int32_t* mask_data = <int32_t*> masks.data
        cdef float* out_data = <float*> outputs.data

        self.processor.execute(in_data, mask_data, out_data, size)

        return outputs

    def get_efficiency(self) -> float:
        '''Get the efficiency of inference.
        Returns:
            float: The efficiency of inference
        '''
        return self.processor.getEfficiency()
