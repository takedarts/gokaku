from libc.stdint cimport int32_t
from libcpp.vector cimport vector
from pyx.move cimport Move


cdef extern from "cpp/Candidate.h" namespace "deepshogi":
    cdef cppclass Candidate:
        Move getMove() const
        int32_t getColor() const
        int32_t getVisits() const
        int32_t getPlayouts() const
        float getPolicy() const
        float getValue() const
        vector[Move] getVariations() const
