from typing import List, Tuple

from libcpp.vector cimport vector
from pyx.move cimport Move
from pyx.pnsearch cimport PnSearchEngine


cdef class NativePnSearch:
    '''PN探索を実行するためのクラス'''
    cdef PnSearchEngine* engine

    def __cinit__(self, nodes: int) -> None:
        '''PN探索オブジェクトを作成する
        Args:
            nodes: 探索で使用する最大ノード数
        '''
        self.engine = new PnSearchEngine(nodes)

    def __dealloc__(self) -> None:
        '''PN探索オブジェクトを破棄する'''
        del self.engine

    def get_checkmate_moves(
        self,
        board: NativeBoard,  # type: ignore
        depth: int,
    ) -> List[Tuple[Tuple[int, int], Tuple[int, int], bool]]:
        '''詰み筋を探索して、着手手順を返す。
        詰み筋が見つからない場合は空のリストを返す。
        Args:
            board (NativeBoard): 盤面オブジェクト
            depth (int): 探索の最大深さ
        Returns:
            List[Tuple[Tuple[int, int], Tuple[int, int], bool]]: 詰み筋の手順のリスト
        '''
        cdef vector[Move] moves = self.engine.getCheckmateMoves(board.board, depth)

        return [
            ((moves[i].getSrc().getX(), moves[i].getSrc().getY()),
             (moves[i].getDst().getX(), moves[i].getDst().getY()),
             moves[i].isPromote())
            for i in range(moves.size())
        ]