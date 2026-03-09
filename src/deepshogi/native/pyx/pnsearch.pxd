from libc.stdint cimport int32_t
from libcpp.vector cimport vector
from pyx.board cimport Board
from pyx.move cimport Move


cdef extern from "cpp/PnSearchEngine.h" namespace "deepshogi":
    cdef cppclass PnSearchEngine:
        PnSearchEngine(int32_t nodes) except +
        vector[Move] getCheckmateMoves(const Board* board, int32_t depth)
