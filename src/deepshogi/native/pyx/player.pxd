from libc.stdint cimport int32_t
from libcpp cimport bool
from libcpp.string cimport string
from libcpp.vector cimport vector
from pyx.board cimport Board
from pyx.candidate cimport Candidate
from pyx.move cimport Move
from pyx.processor cimport Processor


cdef extern from "cpp/Player.h" namespace "deepshogi":
    cdef cppclass Player:
        Player(
            Processor* processor, int32_t threads, int32_t cacheSize,
            int32_t nyugyokuScoreBlack, int32_t nyugyokuScoreWhite, int32_t drawTurn,
            int32_t checkSearchDepth, int32_t checkSearchNode,
            float ucbConstant, float pucbConstantInit, float pucbConstantBase,
            bool evalLeafOnly, int32_t maxVisits) except +
        void initialize(const string& sfen)
        int32_t getColor()
        void play(const Move& move)
        void startEvaluation(
            bool equally, int32_t algorithm, int32_t candidateWidth, int32_t checkNodeDepth,
            float temperature, float noise)
        void waitEvaluation(
            int32_t visits, int32_t playouts, float timelimit, bool stop) nogil
        vector[Candidate] getCandidates()
        void copyBoardTo(Board* board)
        string getDebugInfo()
