from libc.stdint cimport uint8_t
from pyx.move cimport Move


cdef extern from "cpp/Result.h" namespace "deepshogi":
    cdef cppclass Result:
        Result() except +
        Result(const Move& move, uint8_t captured)
        Move getMove() const
        uint8_t getCaptured() const
