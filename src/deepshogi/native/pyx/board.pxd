from libc.stdint cimport int32_t
from libcpp cimport bool
from libcpp.pair cimport pair
from libcpp.string cimport string
from libcpp.vector cimport vector
from pyx.move cimport Move


cdef extern from "cpp/Board.h" namespace "deepshogi":
    cdef cppclass Board:
        Board(int32_t, int32_t, int32_t) except +
        bool play(const Move&)
        void initialize(const string)
        int32_t getColor() const
        int32_t getTurn() const
        int32_t getPiece(int32_t, int32_t) const
        int32_t getMovedPiece(const Move&) const
        int32_t getHandPieceNum(int32_t, int32_t) const
        Move getLastMove() const
        vector[pair[int32_t, int32_t]] getAttackers(int32_t, int32_t) const
        vector[Move] getLegalMoves(bool, bool) const
        vector[Move] getCheckmateMoves(int32_t) const
        bool isNyugyoku(int32_t) const
        bool isCheckmate(int32_t) const
        string getSfen() const
        void getInputs(int32_t*) const
        void getInputs(int32_t*, int32_t, int32_t) const
        void copyFrom(Board*)
        string toString() const
