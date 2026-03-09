from libc.stdint cimport int32_t, uint32_t
from libcpp cimport bool as cpp_bool
from libcpp.string cimport string
from libcpp.vector cimport vector


cdef extern from "cpp/Model.h" namespace "deepshogi":
    cdef cppclass Model:
        @staticmethod
        vector[int32_t] getAvailableGPUs() except +

        Model(string, int32_t, cpp_bool, cpp_bool) except +
        void forward(int32_t*, float*, uint32_t)
        int32_t isCuda()
