from typing import List, Tuple

import numpy as np

from .config import BOARD_SIZE, COLOR_NONE, DEFAULT_DRAW_TURN, DEFAULT_NYUGYOKU_SCORES
from .exception import ShogiException
from .native import NativeBoard


def is_hand_position(pos: Tuple[int, int]) -> bool:
    '''Returns True if the specified coordinates are a hand piece position.
    Args:
        pos (Tuple[int, int]): Coordinates
    Returns:
        bool: True if it is a hand piece position
    '''
    return pos[0] >= BOARD_SIZE


class Board(object):
    def __init__(
        self,
        initial_sfen: str | None = None,
        initial_packed_sfen: np.ndarray | None = None,
        nyugyoku_scores: Tuple[int, int] | int = DEFAULT_NYUGYOKU_SCORES,
        draw_turn: int = DEFAULT_DRAW_TURN,
    ) -> None:
        '''
        Initialize the board object.
        Args:
            initial_sfen (str): SFEN string of the initial board
            initial_packed_sfen (np.ndarray): Huffman encoded SFEN data of the initial board
            nyugyoku_scores (Tuple[int, int]): Points required for nyugyoku declaration
            draw_turn (int): Number of moves for a draw
        '''
        if isinstance(nyugyoku_scores, int):
            nyugyoku_scores = (nyugyoku_scores, nyugyoku_scores)

        self.native = NativeBoard(nyugyoku_scores, draw_turn)

        if initial_sfen is not None:
            self.native.initialize_with_sfen(initial_sfen)
        elif initial_packed_sfen is not None:
            self.native.initialize_with_packed_sfen(initial_packed_sfen)

    def play(
        self,
        src: Tuple[int, int],
        dst: Tuple[int, int],
        promote: bool = False,
        piece: int | None = None,
    ) -> None:
        '''Move a piece.
        Args:
            src (int): Source coordinates
            dst (int): Destination coordinates
            promote (bool): Whether to promote
            piece (int): Type of piece after moving
        '''
        if piece is not None and not is_hand_position(src):
            promote = (self.get_piece(src) != piece)

        if not self.native.play(src, dst, promote):
            raise ShogiException(f'Illegal move: {src} -> {dst} (promote={promote})')

    def get_color(self) -> int:
        '''Get the side to move.
        Returns:
            int: Side to move
        '''
        return self.native.get_color()

    def get_turn(self) -> int:
        '''Get the number of moves played.
        Returns:
            int: Number of moves
        '''
        return self.native.get_turn()

    def get_piece(self, pos: Tuple[int, int]) -> int:
        '''Get the piece at the specified coordinates.
        Args:
            pos (Tuple[int, int]): Coordinates
        Returns:
            int: Type of piece
        '''
        return self.native.get_piece(pos)

    def get_moved_piece(
        self,
        src: Tuple[int, int],
        dst: Tuple[int, int],
        promote: bool,
    ) -> int:
        '''Get the type of piece after making the specified move.
        Args:
            src (Tuple[int, int]): Source coordinates
            dst (Tuple[int, int]): Destination coordinates
            promote (bool): Whether to promote
        Returns:
            int: Type of piece
        '''
        return self.native.get_moved_piece(src, dst, promote)

    def get_hand_piece_num(self, color: int, piece: int) -> int:
        '''Get the number of pieces of the specified type and color.
        Args:
            color (int): Side to move
            piece (int): Type of piece
        Returns:
            int: Number of pieces
        '''
        return self.native.get_hand_piece_num(color, piece)

    def get_attackers(self, pos: Tuple[int, int]) -> List[Tuple[int, int]]:
        '''Get a list of positions of pieces attacking the specified coordinates.
        Args:
            pos (Tuple[int, int]): Coordinates
        Returns:
            List[Tuple[int, int]]: List of positions of attacking pieces
        '''
        return self.native.get_attackers(pos[0], pos[1])

    def get_legal_moves(self) -> List[Tuple[Tuple[int, int], Tuple[int, int], bool]]:
        '''Return legal moves.
        Returns:
            List[Tuple[Tuple[int, int], Tuple[int, int], bool]]: List of legal moves
        '''
        return self.native.get_legal_moves()

    def get_history_moves(self) -> List[Tuple[Tuple[int, int], Tuple[int, int], bool]]:
        '''Return a list of move history.
        Returns:
            List[Tuple[Tuple[int, int], Tuple[int, int], bool]]: List of move history
        '''
        return self.native.get_history_moves()

    def search_check_move(
        self,
        check_search_depth: int = 31,
        check_search_node: int = 10_000,
    ) -> Tuple[Tuple[int, int], Tuple[int, int], bool] | None:
        '''Search for a checkmate sequence and return the first move.
        Returns None if no move is found.
        Args:
            check_search_depth (int): Depth for checkmate search
            check_search_node (int): Number of nodes for checkmate search
        Returns:
            Tuple[Tuple[int, int], Tuple[int, int], bool] | None: Move in the checkmate sequence
        '''
        move = self.native.search_check_move(check_search_depth, check_search_node)

        if move[0][0] == -1:
            return None

        return move

    def is_nyugyoku(self) -> bool:
        '''Determine nyugyoku (entering king).
        Returns:
            bool: True if nyugyoku
        '''
        return self.native.is_nyugyoku()

    def is_checkmate(self) -> bool:
        '''Determine checkmate.
        Returns:
            bool: True if checkmate
        '''
        return self.native.is_checkmate()

    def get_sfen(self) -> str:
        '''Return the current position in SFEN format.
        Returns:
            str: SFEN string representing the current position
        '''
        return self.native.get_sfen()

    def get_packed_sfen(self) -> np.ndarray:
        '''Return the current position as Huffman encoded board information.
        Returns:
            str: Huffman encoded board information
        '''
        return self.native.get_packed_sfen()

    def get_inputs(
        self,
        color: int = COLOR_NONE,
        turn: int | None = None,
    ) -> np.ndarray:
        '''Get the data to input to the inference model.
        Args:
            color (int): Side to move (if COLOR_NONE, use current side)
            turn (int | None): Number of moves (if None, use current number of moves)
        Returns:
            np.ndarray: Input data
        '''
        if color == COLOR_NONE:
            color = self.get_color()

        if turn is None:
            turn = self.get_turn()

        return self.native.get_inputs(color, turn)

    def copy_from(self, board: 'Board') -> None:
        '''Copy the board.
        Args:
            board (Board): Source board to copy from
        '''
        self.native.copy_from(board.native)

    def __str__(self) -> str:
        '''Return the board as a string.'''
        return self.native.dump()

    def __repr__(self) -> str:
        '''Return the board as a string.'''
        return self.__str__()
