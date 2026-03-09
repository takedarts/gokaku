from typing import List, Tuple

from .board import Board
from .native import NativePnSearch


class PnSearch(object):
    def __init__(self, nodes: int) -> None:
        '''Create an object to search for checkmate using the PN search algorithm.
        Args:
            nodes (int): Maximum number of nodes for the search
        '''
        self.native = NativePnSearch(nodes)

    def get_checkmate_moves(
        self,
        board: Board,
        depth: int,
    ) -> List[Tuple[Tuple[int, int], Tuple[int, int], bool]]:
        '''Search for checkmate and return a list of checkmate move sequences.
        Return value is a list of tuples in the format (source coordinates, destination coordinates, promotion).
        Returns an empty list if no checkmate is found.
        Args:
            board (Board): The board position to search
            depth (int): Search depth
        Returns:
            List[Tuple[Tuple[int, int], Tuple[int, int], bool]]: List of checkmate move sequences.
        '''
        return self.native.get_checkmate_moves(board.native, depth)
