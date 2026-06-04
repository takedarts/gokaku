from libc.stdint cimport int16_t, uint8_t
from libcpp cimport bool
from pyx.position cimport Position


cdef extern from "cpp/Move.h" namespace "deepshogi":
    cdef cppclass Move:
        Move() except +
        Move(const Position& src, const Position& dst, bool promote) except +
        Move(int16_t move) except +
        int16_t getValue() const
        Position getSrc() const
        Position getDst() const
        bool isPromote() const
        bool isValid() const


cdef extern from "cpp/MoveResult.h" namespace "deepshogi":
    cdef cppclass MoveResult:
        MoveResult() except +
        MoveResult(const Move& move, uint8_t captured)
        Move getMove() const
        uint8_t getCaptured() const
