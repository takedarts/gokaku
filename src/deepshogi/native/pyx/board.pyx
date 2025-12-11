from typing import List, Tuple

from libc.stdint cimport int32_t
from libcpp cimport bool
from libcpp.pair cimport pair
from libcpp.string cimport string
from libcpp.vector cimport vector

import numpy
cimport numpy

from deepshogi.config import MODEL_INPUT_SIZE


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
        void initializeWithSfen(const string)
        void initializeWithPackedSfen(char *)
        int32_t getColor() const
        int32_t getTurn() const
        int32_t getPiece(int32_t, int32_t) const
        int32_t getMovedPiece(const Move&) const
        int32_t getHandPieceNum(int32_t, int32_t) const
        vector[pair[int32_t, int32_t]] getAttackers(int32_t, int32_t) const
        vector[Move] getLegalMoves() const
        vector[Move] getHistoryMoves() const
        Move searchCheckMove(int32_t, int32_t) const
        bool isNyugyoku() const
        bool isCheckmate() const
        string getSfen() const
        void getPackedSfen(char *) const
        void getInputs(float*) const
        void getInputs(float*, int32_t, int32_t) const
        void copyFrom(Board*)
        string dump() const

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

    def initialize_with_sfen(self, sfen: str) -> None:
        '''
        Initialize board with SFEN string.
        Args:
            sfen (str): SFEN string
        '''
        self.board.initializeWithSfen(sfen.encode('utf-8'))

    def initialize_with_packed_sfen(self, data: numpy.ndarray) -> None:
        '''
        Initialize board with Huffman encoded board information.
        Args:
            data (numpy.ndarray): Huffman encoded board information
        '''
        data = numpy.ascontiguousarray(data, dtype=numpy.uint8)
        assert data.shape[0] == 32

        self.board.initializeWithPackedSfen(<char *>data.data)

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

    def get_legal_moves(self) -> List[Tuple[int, int], Tuple[int, int], bool]:
        '''
        Get list of legal moves.
        Return:
            List[Tuple[int, int], Tuple[int, int], bool]: List of legal moves
        '''
        cdef vector[Move] moves = self.board.getLegalMoves()

        return [
            ((moves[i].getSrcX(), moves[i].getSrcY()),
             (moves[i].getDstX(), moves[i].getDstY()),
             moves[i].isPromote())
            for i in range(moves.size())]

    def get_history_moves(self) -> List[Tuple[int, int], Tuple[int, int], bool]:
        '''
        Get list of move history.
        Return:
            List[Tuple[int, int], Tuple[int, int], bool]: List of move history
        '''
        cdef vector[Move] moves = self.board.getHistoryMoves()

        return [
            ((moves[i].getSrcX(), moves[i].getSrcY()),
             (moves[i].getDstX(), moves[i].getDstY()),
             moves[i].isPromote())
            for i in range(moves.size())]

    def search_check_move(
        self,
        check_search_depth: int,
        check_search_node: int,
    ) -> Tuple[Tuple[int, int], Tuple[int, int], bool]:
        '''
        Search for checkmate sequence and return the first move.
        Args:
            check_search_depth (int): Depth for checkmate search
            check_search_node (int): Number of nodes for checkmate search
        Return:
            Tuple[Tuple[int, int], Tuple[int, int], bool]: First move in checkmate sequence
        '''
        cdef Move move = self.board.searchCheckMove(
            check_search_depth, check_search_node)

        return (
            (move.getSrcX(), move.getSrcY()),
            (move.getDstX(), move.getDstY()),
            move.isPromote())

    def is_nyugyoku(self) -> bool:
        '''
        Return True if nyugyoku declaration is possible.
        Return:
            bool: True if nyugyoku declaration is possible
        '''
        return self.board.isNyugyoku()

    def is_checkmate(self) -> bool:
        '''
        Determine if in checkmate.
        Return:
            bool: True if in checkmate
        '''
        return self.board.isCheckmate()

    def get_sfen(self) -> str:
        '''
        Get SFEN string of the board.
        Return:
            str: SFEN string
        '''
        return self.board.getSfen().decode('utf-8')

    def get_packed_sfen(self) -> numpy.ndarray:
        '''
        Get Huffman encoded board information.
        Return:
            numpy.ndarray: Huffman encoded board information
        '''
        cdef numpy.ndarray[numpy.uint8_t, ndim=1, mode="c"] data = numpy.zeros(
            32, dtype=numpy.uint8)

        self.board.getPackedSfen(<char *>data.data)

        return data

    def get_inputs(self, color: int, turn: int) -> numpy.ndarray:
        '''Get board data to input to inference model.
        Args:
            color (int): Side to move
            turn (int): Number of moves (Specify -1 to execute processing for MCTS search)
        Returns:
            numpy.ndarray: Board data
        '''
        cdef numpy.ndarray[numpy.float32_t, ndim=1, mode="c"] inputs = numpy.empty(
            (MODEL_INPUT_SIZE,), dtype=numpy.float32)

        if turn == -1:
            self.board.getInputs(<float*> &inputs[0])
        else:
            self.board.getInputs(<float*> &inputs[0], color, turn)

        return inputs

    def copy_from(self, board: NativeBoard) -> None:
        '''
        Copy the board.
        Args:
            board (NativeBoard): Source board to copy from
        '''
        self.board.copyFrom(board.board)

    def dump(self) -> str:
        '''
        Get string representation of the board.
        Return:
            str: String representation of the board
        '''
        return self.board.dump().decode('utf-8')
