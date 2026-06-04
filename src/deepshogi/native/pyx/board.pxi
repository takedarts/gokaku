from typing import List, Tuple

from libc.stdint cimport int32_t
from libcpp.vector cimport vector

import numpy
cimport numpy

from deepshogi.config import MODEL_INPUT_PACK_SIZE

from pyx.board cimport Board
from pyx.move cimport Move, MoveResult
from pyx.position cimport Position


cdef class NativeBoard:
    '''A class for managing the state of the board.'''
    cdef Board *board

    def __cinit__(self, nyugyoku_scores: Tuple[int, int], draw_turn: int) -> None:
        '''Creates a board object.
        Args:
            nyugyoku_scores (Tuple[int, int]): Scores for entering-king declaration
            draw_turn (int): Number of moves until a draw
        '''
        self.board = new Board(nyugyoku_scores[0], nyugyoku_scores[1], draw_turn)

    def __dealloc__(self) -> None:
        '''Destroys the board object.'''
        del self.board

    def initialize(self, sfen: str) -> None:
        '''
        Initializes the board with a SFEN-format string.
        Args:
            sfen (str): SFEN-format string
        '''
        self.board.initialize(sfen.encode('utf-8'))

    def play(
        self,
        src: Tuple[int, int],
        dst: Tuple[int, int],
        promote: bool,
    ) -> Tuple[Tuple[int, int], Tuple[int, int], bool, int]:
        '''Moves a piece.
        Returns the result of the move as (source coordinate, destination coordinate, promotion flag, type of captured piece).
        Args:
            src (Tuple[int, int]): Source coordinate
            dst (Tuple[int, int]): Destination coordinate
            promote (bool): True if promoting
        Returns:
            Tuple[Tuple[int, int], Tuple[int, int], bool, int]: Result of the move
        '''
        cdef Position src_pos = Position(src[0], src[1])
        cdef Position dst_pos = Position(dst[0], dst[1])
        cdef Move move = Move(src_pos, dst_pos, promote)
        cdef MoveResult result = self.board.play(move)

        return (
            (result.getMove().getSrc().getX(), result.getMove().getSrc().getY()),
            (result.getMove().getDst().getX(), result.getMove().getDst().getY()),
            result.getMove().isPromote(),
            result.getCaptured())

    def undo(self, src: Tuple[int, int], dst: Tuple[int, int], promote: bool, captured: int) -> None:
        '''Restores the board to the state before the move.
        Args:
            src (Tuple[int, int]): Source coordinate
            dst (Tuple[int, int]): Destination coordinate
            promote (bool): True if the move was a promotion
            captured (int): Type of the captured piece
        '''
        cdef Position src_pos = Position(src[0], src[1])
        cdef Position dst_pos = Position(dst[0], dst[1])
        cdef Move move = Move(src_pos, dst_pos, promote)
        cdef MoveResult result = MoveResult(move, captured)

        self.board.undo(result)

    def get_color(self) -> int:
        '''Returns the current turn color.
        Returns:
            int: Current turn color
        '''
        return self.board.getColor()

    def get_turn(self) -> int:
        '''Returns the current move number.
        Returns:
            int: Current move number
        '''
        return self.board.getTurn()

    def get_piece(self, pos: Tuple[int, int]) -> int:
        '''Returns the piece at the specified coordinate.
        Args:
            pos (Tuple[int, int]): Coordinate
        Returns:
            int: Piece type
        '''
        return self.board.getPiece(Position(pos[0], pos[1]))

    def get_hand_piece_num(self, color: int, piece: int) -> int:
        '''Returns the number of the specified piece in hand.
        Args:
            color (int): Player color
            piece (int): Piece type
        Returns:
            int: Number of pieces in hand
        '''
        return self.board.getHandPieceNum(color, piece)

    def get_last_move(self) -> Tuple[Tuple[int, int], Tuple[int, int], bool]:
        '''Returns the last move made.
        Returns:
            Tuple[Tuple[int, int], Tuple[int, int], bool]: Last move
        '''
        cdef Move move = self.board.getLastMove()
        return (
            (move.getSrc().getX(), move.getSrc().getY()),
            (move.getDst().getX(), move.getDst().getY()),
            move.isPromote())

    def get_attackers(self, x: int, y: int) -> List[Tuple[int,int]]:
        '''Returns the list of pieces attacking the specified coordinate.
        Args:
            x (int): X coordinate
            y (int): Y coordinate
        Returns:
            List[Tuple[int, int]]: List of pieces attacking the specified coordinate
        '''
        cdef vector[Position] attackers = self.board.getAttackers(Position(x, y))
        return [(pos.getX(), pos.getY()) for pos in attackers]

    def get_legal_moves(
        self,
        remove_unpromote: bool,
        check_only: bool,
    ) -> List[Tuple[Tuple[int, int], Tuple[int, int], bool]]:
        '''
        Returns the list of legal moves.
        Args:
            remove_unpromote (bool): True to exclude non-promotion moves for pawn, bishop, and rook
            check_only (bool): True to return only moves that give check
        Returns:
            List[Tuple[Tuple[int, int], Tuple[int, int], bool]]: List of legal moves
        '''
        cdef vector[Move] moves = self.board.getLegalMoves(remove_unpromote, check_only)

        return [
            ((moves[i].getSrc().getX(), moves[i].getSrc().getY()),
             (moves[i].getDst().getX(), moves[i].getDst().getY()),
             moves[i].isPromote())
            for i in range(moves.size())]

    def get_checkmate_moves(
        self,
        depth: int,
    ) -> List[Tuple[Tuple[int, int], Tuple[int, int], bool]]:
        '''
        Returns the sequence of moves for the checkmate line from the current board state.
        Args:
            depth (int): Depth of the checkmate search
        Returns:
            List[Tuple[Tuple[int, int], Tuple[int, int], bool]]: Sequence of moves in the checkmate line
        '''
        cdef vector[Move] moves = self.board.getCheckmateMoves(depth)

        return [
            ((moves[i].getSrc().getX(), moves[i].getSrc().getY()),
             (moves[i].getDst().getX(), moves[i].getDst().getY()),
             moves[i].isPromote())
            for i in range(moves.size())]

    def is_nyugyoku(self, color: int) -> bool:
        '''
        Returns True if entering-king declaration is possible.
        Args:
            color (int): Color of the declaring player
        Returns:
            bool: True if entering-king declaration is possible
        '''
        return self.board.isNyugyoku(color)

    def is_check(self, color: int) -> bool:
        '''
        Determines whether the specified player is in check.
        Args:
            color (int): Color of the player who may be in check
        Returns:
            bool: True if the player is in check
        '''
        return self.board.isCheck(color)

    def get_sfen(self) -> str:
        '''
        Returns the SFEN-format string representation of the board.
        Returns:
            str: SFEN-format string
        '''
        return self.board.getSfen().decode('utf-8')

    def get_inputs(self, color: int) -> numpy.ndarray:
        '''Returns the data to be fed into the model.
        Args:
            color (int): Current turn color
        Returns:
            numpy.ndarray: Data to be fed into the model
        '''
        cdef numpy.ndarray[numpy.int32_t, ndim=1, mode="c"] inputs = numpy.empty(
            (MODEL_INPUT_PACK_SIZE,), dtype=numpy.int32)

        self.board.getInputs(<int32_t*> &inputs[0], color)

        return inputs

    def copy_from(self, board: NativeBoard) -> None:
        '''
        Copies the board state from another board.
        Args:
            board (NativeBoard): Source board to copy from
        '''
        self.board.copyFrom(board.board)

    def to_string(self) -> str:
        '''
        Returns the string representation of the board.
        Returns:
            str: String representation of the board
        '''
        return self.board.toString().decode('utf-8')
