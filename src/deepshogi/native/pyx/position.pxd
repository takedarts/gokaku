from libc.stdint cimport int8_t
from libcpp.string cimport string


cdef extern from "cpp/Position.h" namespace "deepshogi":
    cdef cppclass Position:
        Position() except +
        Position(int8_t x, int8_t y) except +
        Position(int8_t index) except +
        int8_t getIndex() const
        int8_t getX() const
        int8_t getY() const
        string toString() const
