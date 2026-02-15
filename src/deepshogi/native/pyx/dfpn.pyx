cdef extern from "cpp/DfpnEngine.h" namespace "deepshogi":
    cdef cppclass DfpnEngine:
        DfpnEngine(int32_t) except +
        vector[Move] getCheckmateMoves(const Board*, int32_t) nogil


cdef class NativeDfpn:
    cdef DfpnEngine* engine

    def __cinit__(self, nodes: int) -> None:
        '''Create a DFPN engine object.
        Args:
            nodes (int): Maximum number of nodes for search
        '''
        self.engine = new DfpnEngine(nodes)

    def __dealloc__(self) -> None:
        '''Deallocate the DFPN engine object.'''
        del self.engine

    def get_checkmate_moves(
        self,
        board: NativeBoard,
        depth: int,
    ) -> List[Tuple[Tuple[int, int], Tuple[int, int], bool]]:
        '''Search for checkmate sequences and return the move sequence.
        Returns an empty list if no checkmate sequence is found.
        Args:
            board (NativeBoard): Board object
            depth (int): Maximum search depth
        Returns:
            List[Tuple[Tuple[int, int], Tuple[int, int], bool]]: List of moves in the checkmate sequence
        '''
        cdef vector[Move] moves = self.engine.getCheckmateMoves(board.board, depth)

        return [
            ((moves[i].getSrcX(), moves[i].getSrcY()),
             (moves[i].getDstX(), moves[i].getDstY()),
             moves[i].isPromote())
            for i in range(moves.size())
        ]


