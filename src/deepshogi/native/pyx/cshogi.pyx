INITIALIZED = False

cdef extern from "cpp/cshogi/init.hpp" namespace 'deepshogi::cshogi':
    void initTable()


cdef extern from "cpp/cshogi/position.hpp" namespace 'deepshogi::cshogi':
    cdef cppclass Position:
        @staticmethod
        void initZobrist()


cdef extern from "cpp/cshogi/cshogi.h" namespace 'deepshogi::cshogi':
    void HuffmanCodedPos_init()
    void PackedSfen_init()
    void Book_init()


def init_cshogi():
    global INITIALIZED

    if INITIALIZED:
        return

    initTable()
    Position.initZobrist()
    HuffmanCodedPos_init()
    PackedSfen_init()
    Book_init()
    INITIALIZED = True
