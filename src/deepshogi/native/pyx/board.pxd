from libc.stdint cimport int8_t, int16_t, int32_t, uint8_t, uint64_t
from libcpp cimport bool as cpp_bool
from libcpp.pair cimport pair
from libcpp.string cimport string
from libcpp.vector cimport vector
from pyx.move cimport Move
from pyx.position cimport Position
from pyx.result cimport Result


cdef extern from "cpp/Board.h" namespace "deepshogi":
    cdef cppclass Board:
        Board() except +
        Board(int8_t nyugyokuScoreBlack, int8_t nyugyokuScoreWhite, int16_t drawTurn)
        void initialize(const string& sfen)
        Result play(const Move& move) except +
        void undo(const Result& result)
        vector[Position] getAttackers(const Position& position) const
        vector[Move] getLegalMoves(cpp_bool removeUnpromote, cpp_bool checkOnly) const
        vector[Move] getCheckmateMoves(int32_t depth) const
        cpp_bool isNyugyoku(int8_t color) const
        cpp_bool isCheck(int8_t color) const
        string getSfen() const
        void getInputs(int32_t* inputs, int8_t color) const
        void copyFrom(const Board* board)
        string toString() const
        int8_t getColor() const
        int16_t getTurn() const
        int16_t getDrawTurn() const
        uint8_t getPiece(const Position& position) const
        int8_t getHandPieceNum(int8_t color, uint8_t piece) const
        Move getLastMove() const
