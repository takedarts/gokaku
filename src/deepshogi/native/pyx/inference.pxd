from libc.stdint cimport int32_t
from libcpp cimport bool as cpp_bool
from libcpp.string cimport string
from libcpp.vector cimport vector
from pyx.board cimport Board


cdef extern from "cpp/InferenceModel.h" namespace "deepshogi":
    cdef cppclass InferenceModel:
        @staticmethod
        vector[int32_t] getAvailableGPUs() except +

        InferenceModel(string filename, int32_t gpu, cpp_bool fp16, cpp_bool deterministic) except +
        void forward(int32_t* inputs, int32_t* masks, float* outputs, int32_t size) except +
        cpp_bool isCuda()


cdef extern from "cpp/InferenceProcessor.h" namespace "deepshogi":
    cdef cppclass InferenceProcessor:
        InferenceProcessor(
            string model, vector[int32_t] gpus, cpp_bool fp16, cpp_bool deterministic,
            int32_t batchSize, int32_t threadsPerGpu, int32_t cacheSize) except +
        float predict(Board* board) except +
        void execute(int32_t* inputs, int32_t* masks, float* outputs, int32_t size) except +
        float getBatchFillRate() except +
        float getCacheHitRate() except +
