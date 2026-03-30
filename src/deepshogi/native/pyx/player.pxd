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
            Processor*, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t,
            float, float, float, bool, int32_t) except +
        void initialize(const string)
        int32_t getColor()
        void play(const Move&)
        void startEvaluation(bool, int32_t, int32_t, int32_t, float, float)
        void waitEvaluation(int32_t, int32_t, float, bool) nogil
        vector[Candidate] getCandidates()
        void copyBoardTo(Board*)
        string getDebugInfo()
