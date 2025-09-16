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
        void getInputs(float*, int32_t, int32_t) const
        void copyFrom(Board*)
        string dump() const

cdef class NativeBoard:
    cdef Board *board

    def __cinit__(self, nyugyoku_scores: Tuple[int, int], draw_steps: int) -> None:
        '''盤面オブジェクトを作成する。
        Args:
            nyugyoku_scores (Tuple[int, int]): 入玉宣言のスコア
            draw_steps (int): 引き分けまでの手数
        '''
        self.board = new Board(nyugyoku_scores[0], nyugyoku_scores[1], draw_steps)

    def __dealloc__(self) -> None:
        '''盤面オブジェクトを破棄する。'''
        del self.board

    def initialize_with_sfen(self, sfen: str) -> None:
        '''
        SFEN形式の文字列で盤面を初期化する。
        Args:
            sfen (str): SFEN形式の文字列
        '''
        self.board.initializeWithSfen(sfen.encode('utf-8'))

    def initialize_with_packed_sfen(self, data: numpy.ndarray) -> None:
        '''
        ハフマン符号化された盤面情報で盤面を初期化する。
        Args:
            data (numpy.ndarray): ハフマン符号化された盤面情報
        '''
        data = numpy.ascontiguousarray(data, dtype=numpy.uint8)
        assert data.shape[0] == 32

        self.board.initializeWithPackedSfen(<char *>data.data)

    def play(self, src: Tuple[int, int], dst: Tuple[int, int], promote: bool) -> bool:
        '''指し手を適用する。
        Args:
            src (Tuple[int, int]): 移動元の座標
            dst (Tuple[int, int]): 移動先の座標
            promote (bool): 成りの場合True
        Returns:
            bool: 合法手ならTrue
        '''
        return self.board.play(Move(src[0], src[1], dst[0], dst[1], promote))

    def get_color(self) -> int:
        '''手番を取得する。
        Returns:
            int: 手番
        '''
        return self.board.getColor()

    def get_turn(self) -> int:
        '''手数を取得する。
        Returns:
            int: 手数
        '''
        return self.board.getTurn()

    def get_piece(self, pos: Tuple[int, int]) -> int:
        '''指定した座標の駒を取得する。
        Args:
            pos (Tuple[int, int]): 座標
        Returns:
            int: 駒の種類
        '''
        return self.board.getPiece(pos[0], pos[1])

    def get_moved_piece(self, src: Tuple[int, int], dst: Tuple[int, int], promote: bool) -> int:
        '''指定した指し手で動かされた駒を取得する。
        Args:
            src (Tuple[int, int]): 移動元の座標
            dst (Tuple[int, int]): 移動先の座標
            promote (bool): 成りの場合True
        Returns:
            int: 駒の種類
        '''
        return self.board.getMovedPiece(Move(src[0], src[1], dst[0], dst[1], promote))

    def get_hand_piece_num(self, color: int, piece: int) -> int:
        '''指定した持ち駒の数を取得する。
        Args:
            color (int): 手番
            piece (int): 駒の種類
        Returns:
            int: 持ち駒の数
        '''
        return self.board.getHandPieceNum(color, piece)

    def get_attackers(self, x: int, y: int) -> List[Tuple[int,int]]:
        '''指定した座標に利いている駒のリストを取得する。
        Args:
            x (int): X座標
            y (int): Y座標
        Returns:
            List[int]: 駒の種類のリスト
        '''
        cdef vector[pair[int32_t, int32_t]] attackers = self.board.getAttackers(x, y)
        return [(attacker.first, attacker.second) for attacker in attackers]

    def get_legal_moves(self) -> List[Tuple[int, int], Tuple[int, int], bool]:
        '''
        合法手のリストを取得する。
        Return:
            List[Tuple[int, int], Tuple[int, int], bool]: 合法手のリスト
        '''
        cdef vector[Move] moves = self.board.getLegalMoves()

        return [
            ((moves[i].getSrcX(), moves[i].getSrcY()),
             (moves[i].getDstX(), moves[i].getDstY()),
             moves[i].isPromote())
            for i in range(moves.size())]

    def get_history_moves(self) -> List[Tuple[int, int], Tuple[int, int], bool]:
        '''
        着手履歴の一覧を取得する。
        Return:
            List[Tuple[int, int], Tuple[int, int], bool]: 着手履歴の一覧
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
        詰みの手筋を探索して、最初の着手を返す。
        Args:
            check_search_depth (int): 詰み探索の深さ
            check_search_node (int): 詰み探索のノード数
        Return:
            Tuple[Tuple[int, int], Tuple[int, int], bool]: 詰みの手筋の最初の着手
        '''
        cdef Move move = self.board.searchCheckMove(
            check_search_depth, check_search_node)

        return (
            (move.getSrcX(), move.getSrcY()),
            (move.getDstX(), move.getDstY()),
            move.isPromote())

    def is_nyugyoku(self) -> bool:
        '''
        入玉宣言ができるならTrueを返す。
        Return:
            bool: 入玉宣言ができるならTrue
        '''
        return self.board.isNyugyoku()

    def is_checkmate(self) -> bool:
        '''
        王手がかかっているかを判定する。
        Return:
            bool: 王手がかかっていればTrue
        '''
        return self.board.isCheckmate()

    def get_sfen(self) -> str:
        '''
        盤面のSFEN形式の文字列を取得する。
        Return:
            str: SFEN形式の文字列
        '''
        return self.board.getSfen().decode('utf-8')

    def get_packed_sfen(self) -> numpy.ndarray:
        '''
        ハフマン符号化された盤面情報を取得する。
        Return:
            numpy.ndarray: ハフマン符号化された盤面情報
        '''
        cdef numpy.ndarray[numpy.uint8_t, ndim=1, mode="c"] data = numpy.zeros(
            32, dtype=numpy.uint8)

        self.board.getPackedSfen(<char *>data.data)

        return data

    def get_inputs(self, color: int, steps: int) -> numpy.ndarray:
        '''推論モデルに入力する盤面データを取得する。
        Args:
            color (int): 手番
            steps (int): 手数
        Returns:
            numpy.ndarray: 盤面データ
        '''
        cdef numpy.ndarray[numpy.float32_t, ndim=1, mode="c"] inputs = numpy.empty(
            (MODEL_INPUT_SIZE,), dtype=numpy.float32)

        self.board.getInputs(<float*> &inputs[0], color, steps)

        return inputs

    def copy_from(self, board: NativeBoard) -> None:
        '''
        盤面をコピーする。
        Args:
            board (NativeBoard): コピー元の盤面
        '''
        self.board.copyFrom(board.board)

    def dump(self) -> str:
        '''
        盤面の文字列表現を取得する。
        Return:
            str: 盤面の文字列表現
        '''
        return self.board.dump().decode('utf-8')
