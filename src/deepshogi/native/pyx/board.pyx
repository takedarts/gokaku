from typing import List, Tuple

from libc.stdint cimport int32_t
from libcpp cimport bool
from libcpp.pair cimport pair
from libcpp.string cimport string
from libcpp.vector cimport vector

import numpy
cimport numpy

from deepshogi.config import MODEL_INPUT_PACK_SIZE


cdef extern from "cpp/Move.h" namespace "deepshogi":
    cdef cppclass Move:
        Move() except +
        Move(int32_t, int32_t, int32_t, int32_t, bool) except +
        int32_t getSrcX() const
        int32_t getSrcY() const
        int32_t getDstX() const
        int32_t getDstY() const
        bool isPromote() const
        int32_t getValue() const


cdef extern from "cpp/Board.h" namespace "deepshogi":
    cdef cppclass Board:
        Board(int32_t, int32_t, int32_t) except +
        bool play(const Move&)
        void initialize(const string)
        int32_t getColor() const
        int32_t getTurn() const
        int32_t getPiece(int32_t, int32_t) const
        int32_t getMovedPiece(const Move&) const
        int32_t getHandPieceNum(int32_t, int32_t) const
        Move getLastMove() const
        vector[pair[int32_t, int32_t]] getAttackers(int32_t, int32_t) const
        vector[Move] getLegalMoves(bool, bool) const
        vector[Move] getCheckmateMoves(int32_t) const
        bool isNyugyoku(int32_t) const
        bool isCheckmate(int32_t) const
        string getSfen() const
        void getInputs(int32_t*) const
        void getInputs(int32_t*, int32_t, int32_t) const
        void copyFrom(Board*)
        string toString() const


cdef class NativeBoard:
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

    def play(self, src: Tuple[int, int], dst: Tuple[int, int], promote: bool) -> bool:
        '''Apply move.
        Args:
            src (Tuple[int, int]): Source coordinates
            dst (Tuple[int, int]): Destination coordinates
            promote (bool): True if promote
        Returns:
            bool: True if legal move
        '''
        return self.board.play(Move(src[0], src[1], dst[0], dst[1], promote))

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
        return self.board.getPiece(pos[0], pos[1])

    def get_moved_piece(self, src: Tuple[int, int], dst: Tuple[int, int], promote: bool) -> int:
        '''Get piece moved by specified move.
        Args:
            src (Tuple[int, int]): Source coordinates
            dst (Tuple[int, int]): Destination coordinates
            promote (bool): True if promote
        Returns:
            int: Type of piece
        '''
        return self.board.getMovedPiece(Move(src[0], src[1], dst[0], dst[1], promote))

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
            (move.getSrcX(), move.getSrcY()),
            (move.getDstX(), move.getDstY()),
            move.isPromote())

    def get_attackers(self, x: int, y: int) -> List[Tuple[int,int]]:
        '''Get list of pieces attacking specified coordinates.
        Args:
            x (int): X coordinate
            y (int): Y coordinate
        Returns:
            List[int]: List of piece types
        '''
        cdef vector[pair[int32_t, int32_t]] attackers = self.board.getAttackers(x, y)
        return [(attacker.first, attacker.second) for attacker in attackers]

    def get_legal_moves(
        self,
        remove_unpromote: bool,
        checkmate_only: bool,
    ) -> List[Tuple[int, int], Tuple[int, int], bool]:
        '''
        Get list of legal moves.
        Args:
            remove_unpromote (bool): True to remove unpromoted moves for pawn, bishop, and rook
            checkmate_only (bool): True to get only checkmate moves
        Returns:
            List[Tuple[int, int], Tuple[int, int], bool]: List of legal moves
        '''
        cdef vector[Move] moves = self.board.getLegalMoves(remove_unpromote, checkmate_only)

        return [
            ((moves[i].getSrcX(), moves[i].getSrcY()),
             (moves[i].getDstX(), moves[i].getDstY()),
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
            ((moves[i].getSrcX(), moves[i].getSrcY()),
             (moves[i].getDstX(), moves[i].getDstY()),
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

    def is_checkmate(self, color: int) -> bool:
        '''
        Determine if in checkmate.
        Args:
            color (int): Side to move
        Returns:
            bool: True if in checkmate
        '''
        return self.board.isCheckmate(color)

    def get_sfen(self) -> str:
        '''
        Get SFEN string of the board.
        Returns:
            str: SFEN string
        '''
        return self.board.getSfen().decode('utf-8')

    def get_inputs(self, color: int, turn: int) -> numpy.ndarray:
        '''Get board data to input to inference model.
        Args:
            color (int): Side to move
            turn (int): Number of moves (Specify -1 to execute processing for MCTS search)
        Returns:
            numpy.ndarray: Board data
        '''
        cdef numpy.ndarray[numpy.int32_t, ndim=1, mode="c"] inputs = numpy.empty(
            (MODEL_INPUT_PACK_SIZE,), dtype=numpy.int32)

        if turn == -1:
            self.board.getInputs(<int32_t*> &inputs[0])
        else:
            self.board.getInputs(<int32_t*> &inputs[0], color, turn)

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
