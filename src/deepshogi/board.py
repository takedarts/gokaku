from typing import List, Tuple

import numpy as np

from .config import BOARD_SIZE, COLOR_NONE, DEFAULT_DRAW_STEPS, DEFAULT_NYUGYOKU_SCORES
from .exception import ShogiException
from .native import NativeBoard


def is_hand_position(pos: Tuple[int, int]) -> bool:
    '''指定された座標が手駒の位置であればTrueを返す。
    Args:
        pos (Tuple[int, int]): 座標
    Returns:
        bool: 手駒の位置であればTrue
    '''
    return pos[0] >= BOARD_SIZE


class Board(object):
    def __init__(
        self,
        initial_sfen: str | None = None,
        initial_packed_sfen: np.ndarray | None = None,
        nyugyoku_scores: Tuple[int, int] | int = DEFAULT_NYUGYOKU_SCORES,
        draw_steps: int = DEFAULT_DRAW_STEPS,
    ) -> None:
        '''
        盤面オブジェクトを初期化する。
        Args:
            initial_sfen (str): 初期盤面のSFEN形式の文字列
            initial_packed_sfen (np.ndarray): 初期盤面のハフマン符号化されたSFENデータ
            nyugyoku_scores (Tuple[int, int]): 入玉宣言に必要な点数
            draw_steps (int): 引き分けになるまでの手数
        '''
        if isinstance(nyugyoku_scores, int):
            nyugyoku_scores = (nyugyoku_scores, nyugyoku_scores)

        self.native = NativeBoard(nyugyoku_scores, draw_steps)

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
        '''駒を動かす。
        Args:
            src (int): 移動元の座標
            dst (int): 移動先の座標
            promote (bool): 成るかどうか
            piece (int): 移動後の駒の種類
        '''
        if piece is not None and not is_hand_position(src):
            promote = (self.get_piece(src) != piece)

        if not self.native.play(src, dst, promote):
            raise ShogiException(f'Illegal move: {src} -> {dst} (promote={promote})')

    def get_color(self) -> int:
        '''手番を取得する。
        Returns:
            int: 手番
        '''
        return self.native.get_color()

    def get_turn(self) -> int:
        '''手数を取得する。
        Returns:
            int: 手数
        '''
        return self.native.get_turn()

    def get_piece(self, pos: Tuple[int, int]) -> int:
        '''指定された座標の駒を取得する。
        Args:
            pos (Tuple[int, int]): 座標
        Returns:
            int: 駒の種類
        '''
        return self.native.get_piece(pos)

    def get_moved_piece(
        self,
        src: Tuple[int, int],
        dst: Tuple[int, int],
        promote: bool,
    ) -> int:
        '''指定された着手を行った場合の移動後の駒の種類を取得する。
        Args:
            src (Tuple[int, int]): 移動元の座標
            dst (Tuple[int, int]): 移動先の座標
            promote (bool): 成るかどうか
        Returns:
            int: 駒の種類
        '''
        return self.native.get_moved_piece(src, dst, promote)

    def get_hand_piece_num(self, color: int, piece: int) -> int:
        '''指定された色の指定された種類の駒の枚数を取得する。
        Args:
            color (int): 手番
            piece (int): 駒の種類
        Returns:
            int: 駒の枚数
        '''
        return self.native.get_hand_piece_num(color, piece)

    def get_attackers(self, pos: Tuple[int, int]) -> List[Tuple[int, int]]:
        '''指定された座標に利いている駒の位置の一覧を取得する。
        Args:
            pos (Tuple[int, int]): 座標
        Returns:
            List[Tuple[int, int]]: 利いている駒の位置の一覧
        '''
        return self.native.get_attackers(pos[0], pos[1])

    def get_legal_moves(self) -> List[Tuple[Tuple[int, int], Tuple[int, int], bool]]:
        '''合法手を返す。
        Returns:
            List[Tuple[Tuple[int, int], Tuple[int, int], bool]]: 合法手のリスト
        '''
        return self.native.get_legal_moves()

    def get_history_moves(self) -> List[Tuple[Tuple[int, int], Tuple[int, int], bool]]:
        '''着手履歴の一覧を返す。
        Returns:
            List[Tuple[Tuple[int, int], Tuple[int, int], bool]]: 着手履歴の一覧
        '''
        return self.native.get_history_moves()

    def search_check_move(
        self,
        check_search_depth: int = 31,
        check_search_node: int = 10_000,
    ) -> Tuple[Tuple[int, int], Tuple[int, int], bool] | None:
        '''詰み筋を探索して、最初の着手を返す。
        着手が見つからない場合はNoneを返す。
        Args:
            check_search_depth (int): 詰み筋探索の深さ
            check_search_node (int): 詰み筋探索のノード数
        Returns:
            Tuple[Tuple[int, int], Tuple[int, int], bool] | None: 詰み筋の着手
        '''
        move = self.native.search_check_move(check_search_depth, check_search_node)

        if move[0][0] == -1:
            return None

        return move

    def is_nyugyoku(self) -> bool:
        '''入玉判定を行う。
        Returns:
            bool: 入玉していればTrue
        '''
        return self.native.is_nyugyoku()

    def is_checkmate(self) -> bool:
        '''王手判定を行う。
        Returns:
            bool: 王手していればTrue
        '''
        return self.native.is_checkmate()

    def get_sfen(self) -> str:
        '''現在の局面をSFEN形式で返す。
        Returns:
            str: 現在の局面を表すSFEN文字列
        '''
        return self.native.get_sfen()

    def get_packed_sfen(self) -> np.ndarray:
        '''現在の局面をハフマン符号化された盤面情報で返す。
        Returns:
            str: ハフマン符号化された盤面情報
        '''
        return self.native.get_packed_sfen()

    def get_inputs(
        self,
        color: int = COLOR_NONE,
        steps: int | None = None,
    ) -> np.ndarray:
        '''推論モデルに入力するデータを取得する。
        Args:
            color (int): 手番（COLOR_NONEなら現在の手番）
            steps (int | None): 手数（Noneなら現在の手数）
        Returns:
            np.ndarray: 入力データ
        '''
        if color == COLOR_NONE:
            color = self.get_color()

        if steps is None:
            steps = self.get_turn()

        return self.native.get_inputs(color, steps)

    def copy_from(self, board: 'Board') -> None:
        '''盤面をコピーする。
        Args:
            board (Board): コピー元の盤面
        '''
        self.native.copy_from(board.native)

    def __str__(self) -> str:
        '''盤面を文字列で返す。'''
        return self.native.dump()

    def __repr__(self) -> str:
        '''盤面を文字列で返す。'''
        return self.__str__()
