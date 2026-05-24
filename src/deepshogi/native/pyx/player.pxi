from typing import List, Tuple

from libc.stdint cimport int32_t
from libcpp cimport bool
from libcpp.vector cimport vector
from pyx.candidate cimport Candidate
from pyx.move cimport Move
from pyx.player cimport Player
from pyx.position cimport Position


cdef class NativePlayer:
    '''プレイヤとしてゲームの状態管理と着手の決定を行うクラス。'''
    cdef Player* player

    def __cinit__(
        self,
        processor: NativeInferenceProcessor,  # type: ignore
        threads: int,
        max_visits: int,
        nyugyoku_scores: Tuple[int, int],
        draw_turn: int,
        check_search_depth: int,
        check_search_node: int,
        check_node_depth: int,
        ucb_constant: float,
        pucb_constant_init: float,
        pucb_constant_base: float,
    )->None:
        '''プレイヤオブジェクトを初期化する。
        Args:
            processor (NativeInferenceProcessor): 推論プロセッサオブジェクト
            threads (int): スレッド数
            max_visits (int): ノードの最大訪問回数
            nyugyoku_scores (Tuple[int, int]): 入玉宣言に必要となる点数
            draw_turn (int): 引き分けとなるまでの手数
            check_search_depth (int): 詰み探索の深さ
            check_search_node (int): 詰み探索のノード数
            check_node_depth (int): 詰み探索のノードの深さ
            ucb_constant (float): UCBの信頼上限に掛ける定数
            pucb_constant_init (float): PUCBの信頼上限に掛ける定数の初期値
            pucb_constant_base (float): PUCBの信頼上限に掛ける定数の変化値
        '''
        self.player = new Player(
            processor.processor, threads, max_visits,
            nyugyoku_scores[0], nyugyoku_scores[1], draw_turn,
            check_search_depth, check_search_node, check_node_depth,
            ucb_constant, pucb_constant_init, pucb_constant_base)

    def __dealloc__(self):
        del self.player

    def initialize(self, sfen: str) -> None:
        '''対戦状態を初期状態に設定する。
        Args:
            sfen (str): SFEN形式の局面
        '''
        self.player.initialize(sfen.encode('utf-8'))

    def get_color(self) -> int:
        '''手番を取得する。
        Returns:
            int: 手番
        '''
        return self.player.getColor()

    def play(self, src: Tuple[int, int], dst: Tuple[int, int], promote: bool) -> None:
        '''駒を動かす。
        Args:
            src (Tuple[int, int]): 移動元の座標
            dst (Tuple[int, int]): 移動先の座標
            promote (bool): 成るかどうか
        '''
        cdef Position src_pos = Position(src[0], src[1])
        cdef Position dst_pos = Position(dst[0], dst[1])
        cdef Move move = Move(src_pos, dst_pos, promote)
        self.player.play(move)

    def start_evaluation(
        self,
        equally: bool,
        candidate_width: int,
        temperature: float,
        noise: float,
    ) -> None:
        '''評価を開始する。
        Args:
            equally (bool): 探索回数を均等にするならTrue、PUCB等を使用するならFalse
            candidate_width (int): 候補手の探索幅(0ならば探索幅を自動で設定する)
            temperature (float): 探索の温度パラメータ
            noise (float): 探索のガンベルノイズの強さ
        '''
        self.player.startEvaluation(equally, candidate_width, temperature, noise)

    def wait_evaluation(self, visits: int, playouts: int, timelimit: float, stop: bool) -> None:
        '''指定された訪問数とプレイアウト数になるまで待機する。
        Args:
            visits (int): 訪問数
            playouts (int): プレイアウト数
            timelimit (float): 時間制限（秒）
            stop (bool): 停止命令を出すならばTrue
        '''
        cdef int32_t visits_int = visits
        cdef int32_t playouts_int = playouts
        cdef float timelimit_float = timelimit
        cdef bool stop_bool = stop

        with nogil:
            self.player.waitEvaluation(visits_int, playouts_int, timelimit_float, stop_bool)

    def get_candidates(
        self,
    ) -> List[Tuple[Tuple[int, int], Tuple[int, int], int, int, int, float, float, List[int]]]:
        '''候補手の一覧を取得する。
        Returns:
            List[Tuple[Tuple[int, int], Tuple[int, int], int, int, int, float, float, List[int]]]: 候補手の一覧
        '''
        cdef vector[Candidate] candidates = self.player.getCandidates()

        results: List[Tuple[Tuple[int, int], Tuple[int, int], int, int, float, float, List[int]]] = []

        for i in range(candidates.size()):
            src = (candidates[i].getMove().getSrc().getX(), candidates[i].getMove().getSrc().getY())
            dst = (candidates[i].getMove().getDst().getX(), candidates[i].getMove().getDst().getY())
            promote = candidates[i].getMove().isPromote()
            color = candidates[i].getColor()
            visits = candidates[i].getVisits()
            playouts = candidates[i].getPlayouts()
            policy = candidates[i].getPolicy()
            value = candidates[i].getValue()
            variations = candidates[i].getVariations()

            results.append((
                src, dst, promote, color, visits, playouts, policy, value,
                [variations[j].getValue() for j in range(variations.size())],
            ))

        return results

    def copy_board_to(self, board: NativeBoard) -> None:  # type: ignore
        '''指定された盤面オブジェクトに盤面の状態をコピーする。
        Args:
            board (NativeBoard): 盤面オブジェクト
        '''
        self.player.copyBoardTo(board.board)

    def to_string(self) -> str:
        '''プレイヤオブジェクトの状態を表す文字列を取得する。
        Returns:
            str: プレイヤオブジェクトの状態を表す文字列
        '''
        return self.player.toString().decode('utf-8')
