from libc.stdint cimport int32_t
from libcpp cimport bool
from libcpp.string cimport string
from libcpp.vector cimport vector
from pyx.board cimport Board
from pyx.candidate cimport Candidate
from pyx.inference cimport InferenceProcessor
from pyx.move cimport Move


cdef extern from "cpp/Player.h" namespace "deepshogi":
    cdef cppclass Player:
        Player(
            InferenceProcessor* processor, int32_t threads, int32_t searchMaxVisits,
            int32_t nyugyokuScoreBlack, int32_t nyugyokuScoreWhite,
            float sennichitePenalty, int32_t checkSearchDepth, int32_t checkSearchNode,
            int32_t checkNodeDepth, float pucbConstantInit, float pucbConstantBase,
            float pucbMinVisitsRate) except +
        void initialize(const string& sfen, int32_t drawTurn) except +
        int32_t getColor() except +
        void play(const Move& move) except +
        void startEvaluation(
            bool equally, int32_t candidateWidth, float temperature, float noise) except + nogil
        void waitEvaluation(int32_t visits, float timelimit, bool stop) except + nogil
        void stopEvaluation() except + nogil
        vector[Candidate] getCandidates() except + nogil
        int32_t getVisits() except +
        void copyBoardTo(Board* board) except +
        string toString() except +
