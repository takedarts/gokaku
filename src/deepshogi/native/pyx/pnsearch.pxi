from typing import List, Tuple

from libcpp.vector cimport vector
from pyx.move cimport Move
from pyx.pnsearch cimport PnSearchEngine


cdef class NativePnSearch:
    '''Class for executing PN search'''
    cdef PnSearchEngine* engine

    def __cinit__(self, nodes: int) -> None:
        '''Create a PN search object
        Args:
            nodes: Maximum number of nodes to use in search
        '''
        self.engine = new PnSearchEngine(nodes)

    def __dealloc__(self) -> None:
        '''Deallocate the PN search object'''

        del self.engine

    def get_checkmate_moves(
        self,
        board: NativeBoard,  # type: ignore
        depth: int,
    ) -> List[Tuple[Tuple[int, int], Tuple[int, int], bool]]:
        '''Search for checkmate moves and return the sequence of moves.
        If no checkmate moves are found, return an empty list.
        Args:
            board (NativeBoard): Board object
            depth (int): Maximum search depth
        Returns:
            List[Tuple[Tuple[int, int], Tuple[int, int], bool]]: List of the checkmate moves
        '''
        cdef vector[Move] moves = self.engine.getCheckmateMoves(board.board, depth)

        return [
            ((moves[i].getSrc().getX(), moves[i].getSrc().getY()),
             (moves[i].getDst().getX(), moves[i].getDst().getY()),
             moves[i].isPromote())
            for i in range(moves.size())
        ]