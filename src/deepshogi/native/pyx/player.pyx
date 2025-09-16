from typing import List, Tuple

from libc.stdint cimport int32_t
from libcpp cimport bool
from libcpp.string cimport string
from libcpp.vector cimport vector

include "board.pyx"
include "processor.pyx"

cdef extern from "cpp/Candidate.h" namespace "deepshogi":
    cdef cppclass Candidate:
        Move getMove() const
        int32_t getColor() const
        int32_t getVisits() const
        int32_t getPlayouts() const
        float getPolicy() const
        float getValue() const
        vector[Move] getVariations() const


cdef extern from "cpp/Player.h" namespace "deepshogi":
    cdef cppclass Player:
        Player(Processor*, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, bool) except +
        void initialize(const string)
        int32_t getColor()
        void play(const Move&)
        void startEvaluation(bool, bool, int32_t, int32_t, float, float)
        void waitEvaluation(int32_t, int32_t, float, bool) nogil
        vector[Candidate] getCandidates()
        void copyBoardTo(Board*)


cdef class NativePlayer:
    cdef Player* player
    def __cinit__(
        self,
        processor: NativeProcessor,
        threads: int,
        nyugyoku_scores: Tuple[int, int],
        draw_steps: int,
        check_search_depth: int,
        check_search_node: int,
        eval_leaf_only: bool,
    )->None:
        '''プレイヤオブジェクトを初期化する。
        Args:
            processor (NativeProcessor): プロセッサオブジェクト
            threads (int): スレッド数
            nyugyoku_scores (Tuple[int, int]): 入玉宣言に必要となる点数
            draw_steps (int): 引き分けとなるまでの手数
            check_search_depth (int): 詰み探索の深さ
            check_search_node (int): 詰み探索のノード数
            eval_leaf_only (bool): 葉ノードのみ評価するならばTrue
        '''
        self.player = new Player(
            processor.processor, threads,
            nyugyoku_scores[0], nyugyoku_scores[1], draw_steps,
            check_search_depth, check_search_node, eval_leaf_only)

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

    def play(self, src: Tuple[int, int], dst: Tuple[int, int], promote: bool) -> int:
        '''駒を動かす。
        Args:
            src (Tuple[int, int]): 移動元の座標
            dst (Tuple[int, int]): 移動先の座標
            promote (bool): 成るかどうか
        '''
        self.player.play(Move(src[0], src[1], dst[0], dst[1], promote))

    def start_evaluation(
        self,
        equally: bool,
        use_ucb1: bool,
        candidate_width: int,
        check_node_depth: int,
        temperature: float,
        noise: float,
    ) -> None:
        '''評価を開始する。
        Args:
            equality (int): 探索回数を均等にするならTrue、UCB1かPUCBを使用するならFalse
            use_ucb1 (int): 探索先の基準としてUCB1を使用するならTrue、PUCBを使用するならFalse
            candidate_width (int): 候補手の探索幅(0ならば探索幅を自動で設定する)
            check_node_depth (int): 詰み探索を行うノードの最大深さ
            temperature (float): 探索の温度パラメータ
            noise (float): 探索のガンベルノイズの強さ
        '''
        self.player.startEvaluation(
            equally, use_ucb1, candidate_width, check_node_depth, temperature, noise)

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
            src = (candidates[i].getMove().getSrcX(), candidates[i].getMove().getSrcY())
            dst = (candidates[i].getMove().getDstX(), candidates[i].getMove().getDstY())
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

    def copy_board_to(self, board: NativeBoard) -> None:
        '''指定された盤面オブジェクトに盤面の状態をコピーする。
        Args:
            board (NativeBoard): 盤面オブジェクト
        '''
        self.player.copyBoardTo(board.board)
