from libc.stdint cimport int32_t
from libcpp.vector cimport vector
from pyx.board cimport Board
from pyx.move cimport Move


cdef extern from "cpp/DfpnEngine.h" namespace "deepshogi":
    cdef cppclass DfpnEngine:
        DfpnEngine(int32_t) except +
        vector[Move] getCheckmateMoves(const Board*, int32_t) nogil

