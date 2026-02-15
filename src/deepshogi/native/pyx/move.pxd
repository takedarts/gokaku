from libc.stdint cimport int32_t
from libcpp cimport bool


cdef extern from "cpp/Move.h" namespace "deepshogi":
    cdef cppclass Move:
        Move() except +
        Move(int32_t, int32_t, int32_t, int32_t, bool) except +
        int32_t getSrcX() const
        int32_t getSrcY() const
        int32_t getDstX() const
        int32_t getDstY() const
        bool isPromote() const
        int32_t getValue() const
