from typing import List, Tuple

from libc.stdint cimport int32_t
from libcpp.vector cimport vector

import numpy

cimport numpy

from deepshogi.config import MODEL_INPUT_PACK_SIZE

from pyx.board cimport Board
from pyx.move cimport Move
from pyx.position cimport Position
from pyx.result cimport Result


cdef class NativeBoard:
    '''Class to manage the state of the board.'''
    cdef Board *board

    def __cinit__(self, nyugyoku_scores: Tuple[int, int], draw_turn: int) -> None:
        '''Create board object.
        Args:
            nyugyoku_scores (Tuple[int, int]): Nyugyoku declaration score
            draw_turn (int): Number of moves for a draw
        '''
        self.board = new Board(nyugyoku_scores[0], nyugyoku_scores[1], draw_turn)

    def __dealloc__(self) -> None:
        '''Destroy board object.'''
        del self.board

    def initialize(self, sfen: str) -> None:
        '''
        Initialize board with SFEN string.
        Args:
            sfen (str): SFEN string
        '''
        self.board.initialize(sfen.encode('utf-8'))

    def play(
        self,
        src: Tuple[int, int],
        dst: Tuple[int, int],
        promote: bool,
    ) -> Tuple[Tuple[int, int], Tuple[int, int], bool, int]:
        '''Apply move.
        Returns the result of moving a piece:
        (source coordinates, destination coordinates, promotion flag, captured piece type).
        Args:
            src (Tuple[int, int]): Source coordinates
            dst (Tuple[int, int]): Destination coordinates
            promote (bool): True if promote
        Returns:
            Tuple[Tuple[int, int], Tuple[int, int], bool, int]: Result of the move
        '''
        cdef Position src_pos = Position(src[0], src[1])
        cdef Position dst_pos = Position(dst[0], dst[1])
        cdef Move move = Move(src_pos, dst_pos, promote)
        cdef Result result = self.board.play(move)

        return (
            (result.getMove().getSrc().getX(), result.getMove().getSrc().getY()),
            (result.getMove().getDst().getX(), result.getMove().getDst().getY()),
            result.getMove().isPromote(),
            result.getCaptured())

    def undo(self, src: Tuple[int, int], dst: Tuple[int, int], promote: bool, captured: int) -> None:
        '''Return to the state before moving a piece.
        Args:
            src (Tuple[int, int]): Source coordinates
            dst (Tuple[int, int]): Destination coordinates
            promote (bool): True if promote
            captured (int): Type of captured piece
        '''
        cdef Position src_pos = Position(src[0], src[1])
        cdef Position dst_pos = Position(dst[0], dst[1])
        cdef Move move = Move(src_pos, dst_pos, promote)
        cdef Result result = Result(move, captured)

        self.board.undo(result)

    def get_color(self) -> int:
        '''Get side to move.
        Returns:
            int: Side to move
        '''
        return self.board.getColor()

    def get_turn(self) -> int:
        '''Get number of moves played.
        Returns:
            int: Number of moves
        '''
        return self.board.getTurn()

    def get_piece(self, pos: Tuple[int, int]) -> int:
        '''Get piece at specified coordinates.
        Args:
            pos (Tuple[int, int]): Coordinates
        Returns:
            int: Type of piece
        '''
        return self.board.getPiece(Position(pos[0], pos[1]))

    def get_hand_piece_num(self, color: int, piece: int) -> int:
        '''Get number of specified hand pieces.
        Args:
            color (int): Side to move
            piece (int): Type of piece
        Returns:
            int: Number of hand pieces
        '''
        return self.board.getHandPieceNum(color, piece)

    def get_last_move(self) -> Tuple[Tuple[int, int], Tuple[int, int], bool]:
        '''Get the last move.
        Returns:
            Tuple[Tuple[int, int], Tuple[int, int], bool]: Last move
        '''
        cdef Move move = self.board.getLastMove()
        return (
            (move.getSrc().getX(), move.getSrc().getY()),
            (move.getDst().getX(), move.getDst().getY()),
            move.isPromote())

    def get_attackers(self, x: int, y: int) -> List[Tuple[int,int]]:
        '''Get list of pieces attacking specified coordinates.
        Args:
            x (int): X coordinate
            y (int): Y coordinate
        Returns:
            List[int]: List of piece types attacking the specified coordinates
        '''
        cdef vector[Position] attackers = self.board.getAttackers(Position(x, y))
        return [(pos.getX(), pos.getY()) for pos in attackers]

    def get_legal_moves(
        self,
        remove_unpromote: bool,
        check_only: bool,
    ) -> List[Tuple[int, int], Tuple[int, int], bool]:
        '''
        Get list of legal moves.
        Args:
            remove_unpromote (bool): True to remove unpromoted moves for pawn, bishop, and rook
            check_only (bool): True to get only check moves
        Returns:
            List[Tuple[int, int], Tuple[int, int], bool]: List of legal moves
        '''
        cdef vector[Move] moves = self.board.getLegalMoves(remove_unpromote, check_only)

        return [
            ((moves[i].getSrc().getX(), moves[i].getSrc().getY()),
             (moves[i].getDst().getX(), moves[i].getDst().getY()),
             moves[i].isPromote())
            for i in range(moves.size())]

    def get_checkmate_moves(self, depth: int) -> List[Tuple[Tuple[int, int], Tuple[int, int], bool]]:
        '''
        Get checkmate sequence moves from the current board.
        Args:
            depth (int): Depth for checkmate search
        Returns:
            List[Tuple[Tuple[int, int], Tuple[int, int], bool]]: Checkmate sequence moves
        '''
        cdef vector[Move] moves = self.board.getCheckmateMoves(depth)

        return [
            ((moves[i].getSrc().getX(), moves[i].getSrc().getY()),
             (moves[i].getDst().getX(), moves[i].getDst().getY()),
             moves[i].isPromote())
            for i in range(moves.size())]

    def is_nyugyoku(self, color: int) -> bool:
        '''
        Return True if nyugyoku declaration is possible.
        Args:
            color (int): Side to move
        Returns:
            bool: True if nyugyoku declaration is possible
        '''
        return self.board.isNyugyoku(color)

    def is_check(self, color: int) -> bool:
        '''
        Determine if in check.
        Args:
            color (int): Side to move
        Returns:
            bool: True if in check
        '''
        return self.board.isCheck(color)

    def get_sfen(self) -> str:
        '''
        Get SFEN string of the board.
        Returns:
            str: SFEN string
        '''
        return self.board.getSfen().decode('utf-8')

    def get_inputs(self, color: int) -> numpy.ndarray:
        '''Get board data to input to inference model.
        Args:
            color (int): Side to move
        Returns:
            numpy.ndarray: Board data
        '''
        cdef numpy.ndarray[numpy.int32_t, ndim=1, mode="c"] inputs = numpy.empty(
            (MODEL_INPUT_PACK_SIZE,), dtype=numpy.int32)

        self.board.getInputs(<int32_t*> &inputs[0], color)

        return inputs

    def copy_from(self, board: NativeBoard) -> None:
        '''
        Copy the board.
        Args:
            board (NativeBoard): Source board to copy from
        '''
        self.board.copyFrom(board.board)

    def to_string(self) -> str:
        '''
        Get string representation of the board.
        Returns:
            str: String representation of the board
        '''
        return self.board.toString().decode('utf-8')
