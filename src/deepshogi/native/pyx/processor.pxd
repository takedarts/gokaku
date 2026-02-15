from libc.stdint cimport int32_t
from libcpp cimport bool
from libcpp.string cimport string
from libcpp.vector cimport vector


cdef extern from "cpp/Processor.h" namespace "deepshogi":
    cdef cppclass Processor:
        Processor(string, vector[int32_t], int32_t, bool, bool, int32_t) except +
        void execute(int32_t*, float*, int32_t)
