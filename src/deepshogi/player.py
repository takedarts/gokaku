import logging
import random
from typing import Dict, List, Tuple

from .board import Board, is_hand_position
from .config import (BOARD_SIZE, COLOR_BLACK, COLOR_NONE, COLOR_WHITE,
                     DEFAULT_ALLOWED_REPEATS, DEFAULT_CHECK_SEARCH_DEPTH,
                     DEFAULT_CHECK_SEARCH_NODE, DEFAULT_DRAW_STEPS,
                     DEFAULT_INITIAL_SFEN, DEFAULT_NYUGYOKU_SCORES,
                     RESULT_MAX_MOVES, RESULT_NONE, RESULT_NYUGYOKU,
                     RESULT_SENNICHITE, RESULT_TSUMI, get_color_name,
                     get_opposite_color)
from .native import NativePlayer
from .processor import Processor

LOGGER = logging.getLogger(__name__)


def parse_cshogi_move16(move: int) -> Tuple[Tuple[int, int], Tuple[int, int], bool]:
    '''cshogiの着手表現を解析する。
    Args:
        move (int): cshogiの着手表現
    Returns:
        Tuple[Tuple[int,int], Tuple[int,int], bool]: 移動元、移動先、成るならTrue
    '''
    src = (move >> 7) & 0x7f
    dst = (move >> 0) & 0x7f
    promote = (move & 0x4000) != 0

    return (
        (src // BOARD_SIZE, src % BOARD_SIZE),
        (dst // BOARD_SIZE, dst % BOARD_SIZE),
        promote)


class Candidate(object):
    def __init__(
        self,
        src: Tuple[int, int],
        dst: Tuple[int, int],
        promote: bool,
        color: int,
        visits: int,
        playouts: int,
        policy: float,
        value: float,
        variations: List[int],
    ) -> None:
        '''候補手オブジェクトを初期化する。
        Args:
            src (Tuple[int, int]): 移動元の座標
            dst (Tuple[int, int]): 移動先の座標
            promote (bool): 成るならTrue
            color (int): 手番
            visits (int): 訪問回数
            playouts (int): プレイアウト回数
            policy (float): 予想される着手確率
            value (float): 予測される勝率
            variations (List[int]): 予想進行(cshogiの着手表現)
        '''
        self.src = src
        self.dst = dst
        self.promote = promote
        self.color = color
        self.visits = visits
        self.playouts = playouts
        self.policy = policy
        self.value = value
        self.variations = [parse_cshogi_move16(v) for v in variations]

    @property
    def win_chance(self) -> float:
        return self.value * self.color * 0.5 + 0.5

    @property
    def win_chance_lcb(self) -> float:
        win_chance = self.win_chance

        if win_chance == 1.0:
            return 1.0
        elif win_chance > 0.0:
            return win_chance - 1.96 * 0.25 / (self.visits + 1)**0.5
        else:
            return -1.0

    def __str__(self) -> str:
        return (
            f'Candidate('
            f'src={self.src}, dst={self.dst}, promote={self.promote},'
            f' color={get_color_name(self.color)},'
            f' visits={self.visits}, playouts={self.playouts},'
            f' policy={self.policy:.2f}, value={self.value:.2f},'
            f' win_chance={self.win_chance:.2f},'
            f' win_chance_lcb={self.win_chance_lcb:.2f}')

    def __repr__(self) -> str:
        return str(self)


class Referee(object):
    '''ゲーム結果を判定するクラス'''

    def __init__(
        self,
        allowed_repeats: int = DEFAULT_ALLOWED_REPEATS,
        draw_steps: int = DEFAULT_DRAW_STEPS,
    ) -> None:
        '''審判オブジェクトを初期化する。
        Args:
            allowed_repeats (int): 許容される同一局面の繰り返し回数（既定値は3回）
            draw_steps (int): 引き分けとする手数（既定値は512手）
        '''
        self.allowed_repeats = allowed_repeats
        self.draw_steps = draw_steps

        self.board_repeats: Dict[Tuple, Tuple[int, int]] = {}
        self.check_counts = [0, 0]

    def clear(self) -> None:
        '''状態をクリアする。'''
        self.board_repeats.clear()
        self.check_counts = [0, 0]

    def update(self, board: Board) -> None:
        '''状態を更新する。'''
        # 現在の盤面を履歴に登録する
        repeat_key = tuple(board.get_packed_sfen())
        repeat_values = self.board_repeats.get(repeat_key, (0, board.get_turn()))
        self.board_repeats[repeat_key] = (repeat_values[0] + 1, repeat_values[1])

        # 王手の場合は連続回数をカウントする
        color_index = 0 if board.get_color() == COLOR_WHITE else 1

        if board.is_checkmate():
            self.check_counts[color_index] += 1
        else:
            self.check_counts[color_index] = 0

    def judge(self, board: Board) -> Tuple[bool, int, int]:
        '''勝敗を判定する。
        Args:
            board (Board): 盤面データ
        Returns:
            Tuple[bool, int, int]: ゲームが終了していればTrue, 勝者の手番, 終局理由
        '''
        # 合法手がない場合は詰みと判定する
        if len(board.get_legal_moves()) == 0:
            return True, get_opposite_color(board.get_color()), RESULT_TSUMI

        # 入玉宣言できる場合は入玉宣言による勝ちと判定する
        if board.is_nyugyoku():
            return True, board.get_color(), RESULT_NYUGYOKU

        # 引き分けとする手数を超えた場合は引き分けと判定する
        if board.get_turn() >= self.draw_steps:
            return True, COLOR_NONE, RESULT_MAX_MOVES

        # 千日手かどうかを判定する
        repeat_key = tuple(board.get_packed_sfen())
        repeat_values = self.board_repeats.get(repeat_key, (0, board.get_turn()))
        repeat_count, repeat_turn = repeat_values
        sennichite = (repeat_count >= self.allowed_repeats)

        if sennichite:
            color_index = 0 if board.get_color() == COLOR_WHITE else 1
            check_count = self.check_counts[color_index]
            sennichioute = (check_count >= (board.get_turn() - repeat_turn) // 2)

            if sennichioute:
                return True, board.get_color(), RESULT_SENNICHITE
            else:
                return True, COLOR_NONE, RESULT_SENNICHITE

        # それ以外はゲームが続行中と判定する
        return False, COLOR_NONE, RESULT_NONE

    def get_repeats(self, board: Board) -> int:
        '''同一局面の繰り返し回数を返す。
        Args:
            board (Board): 盤面データ
        Returns:
            int: 同一局面の繰り返し回数
        '''
        repeat_key = tuple(board.get_packed_sfen())
        repeat_values = self.board_repeats.get(repeat_key, (0, 0))

        return repeat_values[0]


class Player(object):
    def __init__(
        self,
        processor: Processor,
        threads: int = 1,
        initial_sfen: str = DEFAULT_INITIAL_SFEN,
        nyugyoku_scores: Tuple[int, int] = DEFAULT_NYUGYOKU_SCORES,
        draw_steps: int = DEFAULT_DRAW_STEPS,
        check_search_depth: int = DEFAULT_CHECK_SEARCH_DEPTH,
        check_search_node: int = DEFAULT_CHECK_SEARCH_NODE,
        eval_leaf_only: bool = False,
        allowed_repeats: int = DEFAULT_ALLOWED_REPEATS,
        check_next_repeats: bool = True,
    ) -> None:
        '''競技者オブジェクトを初期化する。
        Args:
            processor (Processor): 計算管理オブジェクト
            threads (int): 使用するスレッド数
            initial_sfen (str): SFEN形式の初期盤面
            nyugyoku_scores (Tuple[int, int]): 入玉宣言に必要となる点数
            draw_steps (int): 引き分けとする手数
            check_search_depth (int): 詰み探索の深さ
            check_search_node (int): 詰み探索のノード数
            eval_leaf_only (bool): 葉ノードのみ評価するならTrue
            allowed_repeats (int): 許容される同一局面の繰り返し回数（既定値は3回）
            check_next_repeats (bool): 次の手番の千日手を判定する場合はTrue
        '''
        # プロセッサオブジェクトが破棄されないように参照を保持する
        self.processor = processor

        # ネイティブオブジェクトを作成する
        self.native = NativePlayer(
            processor.native, threads, nyugyoku_scores, draw_steps,
            check_search_depth, check_search_node, eval_leaf_only)

        self.native.initialize(initial_sfen)

        # ゲームの審判オブジェクトを作成する
        self.referee = Referee(
            allowed_repeats=allowed_repeats, draw_steps=draw_steps)
        self.check_next_repeats = check_next_repeats

    def initialize(self, sfen: str = DEFAULT_INITIAL_SFEN) -> None:
        '''状態を初期化する。
        Args:
            sfen (str): SFEN形式の初期盤面
        '''
        # ponderingしている場合は停止する
        self.native.wait_evaluation(0, 0, 0.0, True)

        # 初期化する
        self.native.initialize(sfen)
        self.referee.clear()

    def play(
        self,
        src: Tuple[int, int],
        dst: Tuple[int, int],
        promote: bool = False,
        piece: int | None = None,
    ) -> None:
        '''駒を動かす。
        Args:
            src (Tuple[int, int]): 移動元の座標
            dst (Tuple[int, int]): 移動先の座標
            promote (bool): 成るならTrue
            piece (int): 移動後の駒の種類
        '''
        # 移動後の駒の種類が指定されている場合は成るかどうかを駒の種類から判定する
        if piece is not None and not is_hand_position(src):
            promote = (self.get_board().get_piece(src) != piece)

        # 千日手判定オブジェクトを更新する
        self.referee.update(self.get_board())

        # 駒を動かす
        self.native.play(src, dst, promote)

    def get_random(
        self,
        width: int = 16,
        temperature: float = 1.0,
        delta: float = 0.1,
        timelimit: float = 120.0,
    ) -> Candidate:
        '''ランダムな着手を返す。
        Args:
            width (int): 候補手の数
            temperature (float): 温度パラメータ
            delta (float): 許容できる勝率低下値
            timelimit (float): 制限時間（秒）
        Returns:
            Candidate: 候補手
        '''
        # 盤面を評価する
        self.native.start_evaluation(True, False, width, 0, 1.0, 0.0)
        self.native.wait_evaluation(width + 1, 0, timelimit, True)

        # 候補手の一覧を作成する
        candidates = [Candidate(*c) for c in self.native.get_candidates()]

        # 予想勝率の最大値を取得する
        max_win_chance = max(c.win_chance for c in candidates)

        # 予想勝率の最大値との差が0.05以上の予想勝率となっている候補手を除外する
        candidates = [c for c in candidates if c.win_chance >= max_win_chance - delta]

        # policyの値を選択確率に変換する
        probs = [c.policy**(1 / max(temperature, 1e-3)) for c in candidates]

        # ランダムに選択した候補手を返す
        return random.choices(candidates, weights=probs, k=1)[0]

    def evaluate(
        self,
        visits: int,
        playouts: int = 0,
        timelimit: float = 120.0,
        equally: bool = False,
        use_ucb1: bool = False,
        candidate_width: int = 0,
        check_node_depth: int = 2,
        temperature: float = 1.0,
        noise: float = 0.0,
        sennichite_penalty: float = 0.0,
        criterion: str = 'lcb',
        ponder: bool = False,
    ) -> List[Candidate]:
        '''盤面を評価する。
        Args:
            visits (int): 訪問数の目標値
            playouts (int): プレイアウトの目標値
            timelimit (float): 制限時間（秒）
            equally (bool): 探索回数を均等にする場合はTrue、UCB1かPUCBを使う場合はFalse
            use_ucb1 (bool): 探索先の基準としてUCB1を使う場合はTrue、PUCBを使う場合はFalse
            candidate_width (int): 候補手の探索幅（0の場合は探索幅を自動的に調整される）
            check_node_depth (int): 詰み探索を行うノードの最大深さ
            temperature (float): 探索の温度パラメータ
            noise (float): 探索のガンベルノイズの強さ
            sennichite_penalty (float): 千日手の評価値に設定するペナルティ
            criterion (str): 候補手優先度の基準（'lcb'、'visits'のいずれか）
            ponder (bool): 探索を継続する場合はTrue
        Returns:
            List[Candidate]: 候補手の一覧
        '''
        # 盤面を評価する
        LOGGER.debug(
            'Evaluation: %d visits, %d playouts, %.1f seconds',
            visits, playouts, timelimit)
        self.native.start_evaluation(
            equally, use_ucb1, candidate_width, check_node_depth, temperature, noise)
        self.native.wait_evaluation(visits, playouts, timelimit, not ponder)

        # 候補手の一覧を作成する
        candidates = [Candidate(*c) for c in self.native.get_candidates()]

        # 勝敗を判定して評価値に反映する
        for candidate in candidates:
            # 候補手を進めた盤面を作成する
            board = self.get_board()
            board.play(candidate.src, candidate.dst, candidate.promote)

            # 候補手を進めた盤面で終了判定を行う
            game_over, winner, result = self.referee.judge(board)

            # ゲームが終了している場合は勝者の手番を評価値に設定する
            # 引き分け（千日手）の場合は指定されたペナルティを設定する
            if game_over:
                if result == RESULT_SENNICHITE and winner == COLOR_NONE:
                    candidate.value = get_opposite_color(candidate.color) * sennichite_penalty
                else:
                    candidate.value = winner

                continue

            # 次の相手の手番での引き分け（千日手）を判定しない場合は次の候補手に進む
            if not self.check_next_repeats:
                continue

            # 次の相手の手番で引き分け（千日手）の場合も指定されたペナルティを設定する
            next_board = Board()

            for move in board.get_legal_moves():
                next_board.copy_from(board)
                next_board.play(*move)

                repeats = self.referee.get_repeats(next_board)

                if repeats < self.referee.allowed_repeats:
                    continue
                elif candidate.color == COLOR_BLACK:
                    candidate.value = min(candidate.value, -1 * sennichite_penalty)
                    break
                else:
                    candidate.value = max(candidate.value, sennichite_penalty)
                    break

        # 候補手をソートする
        if criterion == 'visits':
            candidates.sort(key=lambda cand: cand.visits, reverse=True)
        elif criterion == 'lcb':
            candidates.sort(key=lambda cand: cand.win_chance_lcb, reverse=True)
        else:
            raise ValueError(f'Unknown criterion: {criterion}')

        # ログを出力する
        if LOGGER.isEnabledFor(logging.DEBUG):
            LOGGER.debug('Evaluation: %d visits', sum(c.visits for c in candidates))
            for candidate in candidates:
                LOGGER.debug(candidate)

        # 候補手の一覧を返す
        return candidates

    def stop_evaluation(self) -> None:
        '''ponderingしている場合は停止する。'''
        self.native.wait_evaluation(0, 0, 0.0, True)

    def get_color(self) -> int:
        '''手番を返す。
        Returns:
            int: 手番
        '''
        return self.native.get_color()

    def get_board(self) -> Board:
        '''盤面データを返す。
        Returns:
            Board: 盤面データ
        '''
        board = Board()

        self.native.copy_board_to(board.native)

        return board
