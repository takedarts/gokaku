from typing import List
from libc.stdint cimport int32_t

import numpy
cimport numpy

from deepshogi.config import MODEL_OUTPUT_SIZE
from pyx.model cimport Model


cdef class NativeModel:
    cdef Model *model

    @staticmethod
    def get_available_gpus() -> List[int]:
        '''Get the list of available GPU IDs.
        Returns:
            list[int]: List of GPU IDs
        '''
        return Model.getAvailableGPUs()

    def __cinit__(self, model: str, gpu: int, fp16: bool, deterministic: bool) -> None:
        '''Create model object.
        Args:
            model (str): Path to the model file
            gpu (int): ID of GPU to use
            fp16 (bool): True to use FP16 computation
            deterministic (bool): If True, results will be reproducible
        '''
        self.model = new Model(model.encode('utf-8'), gpu, fp16, deterministic)

    def __dealloc__(self) -> None:
        del self.model

    def forward(self, inputs: numpy.ndarray) -> numpy.ndarray:
        '''Execute inference.
        Args:
            inputs (numpy.ndarray): Input data
        Returns:
            numpy.ndarray: Output data
        '''
        cdef numpy.ndarray[numpy.float32_t, ndim=2, mode="c"] outputs = numpy.zeros(
            (inputs.shape[0], MODEL_OUTPUT_SIZE), dtype=numpy.float32)

        cdef int size = inputs.shape[0]
        cdef int32_t* in_data = <int32_t*> inputs.data
        cdef float* out_data = <float*> outputs.data

        self.model.forward(in_data, out_data, size)

        return outputs

    def is_cuda(self) -> bool:
        '''Check if CUDA is being used.
        Returns:
            bool: True if CUDA is being used
        '''
        return self.model.isCuda() != 0
