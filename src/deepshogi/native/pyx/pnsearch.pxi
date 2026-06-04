from typing import List, Tuple

from libcpp.vector cimport vector
from pyx.move cimport Move
from pyx.pnsearch cimport PnSearchEngine


cdef class NativePnSearch:
    '''Class for running PN search.'''
    cdef PnSearchEngine* engine

    def __cinit__(self, nodes: int) -> None:
        '''Creates a PN search object.
        Args:
            nodes: Maximum number of nodes used in the search
        '''
        self.engine = new PnSearchEngine(nodes)

    def __dealloc__(self) -> None:
        '''Destroys the PN search object.'''
        del self.engine

    def get_checkmate_moves(
        self,
        board: NativeBoard,  # type: ignore
        depth: int,
    ) -> List[Tuple[Tuple[int, int], Tuple[int, int], bool]]:
        '''Searches for a checkmate sequence and returns the move sequence.
        Returns an empty list if no checkmate sequence is found.
        Args:
            board (NativeBoard): Board object
            depth (int): Maximum search depth
        Returns:
            List[Tuple[Tuple[int, int], Tuple[int, int], bool]]: List of moves in the checkmate sequence
        '''
        cdef vector[Move] moves = self.engine.getCheckmateMoves(board.board, depth)

        return [
            ((moves[i].getSrc().getX(), moves[i].getSrc().getY()),
             (moves[i].getDst().getX(), moves[i].getDst().getY()),
             moves[i].isPromote())
            for i in range(moves.size())
        ]