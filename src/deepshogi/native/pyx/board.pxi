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
    '''盤面の状態を管理するクラス。'''
    cdef Board *board

    def __cinit__(self, nyugyoku_scores: Tuple[int, int], draw_turn: int) -> None:
        '''盤面オブジェクトを作成する。
        Args:
            nyugyoku_scores (Tuple[int, int]): 入玉宣言のスコア
            draw_turn (int): 引き分けまでの手数
        '''
        self.board = new Board(nyugyoku_scores[0], nyugyoku_scores[1], draw_turn)

    def __dealloc__(self) -> None:
        '''盤面オブジェクトを破棄する。'''
        del self.board

    def initialize(self, sfen: str) -> None:
        '''
        SFEN形式の文字列で盤面を初期化する。
        Args:
            sfen (str): SFEN形式の文字列
        '''
        self.board.initialize(sfen.encode('utf-8'))

    def play(
        self,
        src: Tuple[int, int],
        dst: Tuple[int, int],
        promote: bool,
    ) -> Tuple[Tuple[int, int], Tuple[int, int], bool, int]:
        '''駒を動かす。
        駒を動かした結果である(移動元の座標、移動先の座標、成りの有無、取った駒の種類)を返す。
        Args:
            src (Tuple[int, int]): 移動元の座標
            dst (Tuple[int, int]): 移動先の座標
            promote (bool): 成りの場合True
        Returns:
            Tuple[Tuple[int, int], Tuple[int, int], bool, int]: 駒を動かした結果
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
        '''駒を動かす前の状態に戻す。
        Args:
            src (Tuple[int, int]): 移動元の座標
            dst (Tuple[int, int]): 移動先の座標
            promote (bool): 成りの場合True
            captured (int): 取った駒の種類
        '''
        cdef Position src_pos = Position(src[0], src[1])
        cdef Position dst_pos = Position(dst[0], dst[1])
        cdef Move move = Move(src_pos, dst_pos, promote)
        cdef MoveResult result = MoveResult(move, captured)

        self.board.undo(result)

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
        return self.board.getPiece(Position(pos[0], pos[1]))

    def get_hand_piece_num(self, color: int, piece: int) -> int:
        '''指定した持ち駒の数を取得する。
        Args:
            color (int): 手番
            piece (int): 駒の種類
        Returns:
            int: 持ち駒の数
        '''
        return self.board.getHandPieceNum(color, piece)

    def get_last_move(self) -> Tuple[Tuple[int, int], Tuple[int, int], bool]:
        '''最後の着手を取得する。
        Returns:
            Tuple[Tuple[int, int], Tuple[int, int], bool]: 最後の着手
        '''
        cdef Move move = self.board.getLastMove()
        return (
            (move.getSrc().getX(), move.getSrc().getY()),
            (move.getDst().getX(), move.getDst().getY()),
            move.isPromote())

    def get_attackers(self, x: int, y: int) -> List[Tuple[int,int]]:
        '''指定した座標に利いている駒のリストを取得する。
        Args:
            x (int): X座標
            y (int): Y座標
        Returns:
            List[Tuple[int, int]]: 指定した座標に利いている駒のリスト
        '''
        cdef vector[Position] attackers = self.board.getAttackers(Position(x, y))
        return [(pos.getX(), pos.getY()) for pos in attackers]

    def get_legal_moves(
        self,
        remove_unpromote: bool,
        check_only: bool,
    ) -> List[Tuple[Tuple[int, int], Tuple[int, int], bool]]:
        '''
        合法手のリストを取得する。
        Args:
            remove_unpromote (bool): 歩、角、飛車の不成の手を削除する場合はTrue
            check_only (bool): 王手のみの手を取得する場合はTrue
        Returns:
            List[Tuple[Tuple[int, int], Tuple[int, int], bool]]: 合法手のリスト
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
        現在の盤面の詰み筋の着手手順を取得する。
        Args:
            depth (int): 詰み探索の深さ
        Returns:
            List[Tuple[Tuple[int, int], Tuple[int, int], bool]]: 詰み筋の着手手順
        '''
        cdef vector[Move] moves = self.board.getCheckmateMoves(depth)

        return [
            ((moves[i].getSrc().getX(), moves[i].getSrc().getY()),
             (moves[i].getDst().getX(), moves[i].getDst().getY()),
             moves[i].isPromote())
            for i in range(moves.size())]

    def is_nyugyoku(self, color: int) -> bool:
        '''
        入玉宣言ができるならTrueを返す。
        Args:
            color (int): 宣言側の手番
        Returns:
            bool: 入玉宣言ができるならTrue
        '''
        return self.board.isNyugyoku(color)

    def is_check(self, color: int) -> bool:
        '''
        王手がかかっているかを判定する。
        Args:
            color (int): 王手をかけられている側の色
        Returns:
            bool: 王手がかかっていればTrue
        '''
        return self.board.isCheck(color)

    def get_sfen(self) -> str:
        '''
        盤面のSFEN形式の文字列を取得する。
        Returns:
            str: SFEN形式の文字列
        '''
        return self.board.getSfen().decode('utf-8')

    def get_inputs(self, color: int) -> numpy.ndarray:
        '''モデルに入力するデータを取得する。
        Args:
            color (int): 手番
        Returns:
            numpy.ndarray: モデルに入力するデータ
        '''
        cdef numpy.ndarray[numpy.int32_t, ndim=1, mode="c"] inputs = numpy.empty(
            (MODEL_INPUT_PACK_SIZE,), dtype=numpy.int32)

        self.board.getInputs(<int32_t*> &inputs[0], color)

        return inputs

    def copy_from(self, board: NativeBoard) -> None:
        '''
        盤面をコピーする。
        Args:
            board (NativeBoard): コピー元の盤面
        '''
        self.board.copyFrom(board.board)

    def to_string(self) -> str:
        '''
        盤面の文字列表現を取得する。
        Returns:
            str: 盤面の文字列表現
        '''
        return self.board.toString().decode('utf-8')
