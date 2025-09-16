import logging
import sys
from threading import Thread
from typing import Any, Callable, Dict, List, TextIO, Tuple

from deepshogi.board import Board

from .config import (AUTHOR, BOARD_SIZE, COLOR_BLACK, COLOR_WHITE,
                     DEFAULT_INITIAL_SFEN, DEFAULT_NYUGYOKU_SCORES, NAME,
                     PIECE_HAND_BISHOP, PIECE_HAND_GOLD, PIECE_HAND_KNIGHT,
                     PIECE_HAND_LANCE, PIECE_HAND_PAWN, PIECE_HAND_ROOK,
                     PIECE_HAND_SILVER, VERSION, get_color_name,
                     get_opposite_color, get_shogi_score)
from .exception import ShogiException
from .player import Candidate, Player
from .processor import Processor

LOGGER = logging.getLogger(__name__)

USI_HAND_CHAR_TO_PIECE = {
    b'P'[0]: PIECE_HAND_PAWN,
    b'L'[0]: PIECE_HAND_LANCE,
    b'N'[0]: PIECE_HAND_KNIGHT,
    b'S'[0]: PIECE_HAND_SILVER,
    b'B'[0]: PIECE_HAND_BISHOP,
    b'R'[0]: PIECE_HAND_ROOK,
    b'G'[0]: PIECE_HAND_GOLD,
}

USI_HAND_PIECE_TO_CHAR = {
    v: c for c, v in USI_HAND_CHAR_TO_PIECE.items()}


def usi_string_to_move(sfen: str) -> Tuple[Tuple[int, int], Tuple[int, int], bool]:
    '''USI形式の着手文字列を解析して着手情報を返す。
    Args:
        sfen (str): USI形式の着手文字列
    Returns:
        Tuple[Tuple[int,int], Tuple[int,int],bool]: (移動元座標, 移動先座標, 成りの場合はTrue)
    '''
    b = sfen.encode('utf-8')

    # 1文字目が数字なら駒の移動
    if 49 <= b[0] <= 57:
        src = (b[0] - 49, b[1] - 97)
        dst = (b[2] - 49, b[3] - 97)
        promote = (len(b) >= 5 and b[4] == 43)
    # それ以外なら手駒を打つ
    elif b[0] in USI_HAND_CHAR_TO_PIECE:
        src = (BOARD_SIZE, USI_HAND_CHAR_TO_PIECE[b[0]])
        dst = (b[2] - 49, b[3] - 97)
        promote = False
    else:
        raise ShogiException('invalid move: {}'.format(sfen))

    return (src, dst, promote)


def usi_move_to_string(src: Tuple[int, int], dst: Tuple[int, int], promote: bool) -> str:
    '''着手情報をUSI形式の着手文字列に変換する。
    Args:
        src (Tuple[int,int]): 移動元座標
        dst (Tuple[int,int]): 移動先座標
        promote (bool): 成る場合はTrue
    Returns:
        str: USI形式の着手文字列
    '''
    # 駒の移動の場合
    if src[0] != BOARD_SIZE:
        src_str = chr(src[0] + 49) + chr(src[1] + 97)
        dst_str = chr(dst[0] + 49) + chr(dst[1] + 97)
        promote_str = '+' if promote else ''
    # 手駒を打つ場合
    elif src[0] == BOARD_SIZE and src[1] in USI_HAND_PIECE_TO_CHAR:
        src_str = chr(USI_HAND_PIECE_TO_CHAR[src[1]]) + '*'
        dst_str = chr(dst[0] + 49) + chr(dst[1] + 97)
        promote_str = ''
    else:
        raise ShogiException(f'invalid move: {src}, {dst}, {promote}')

    return f'{src_str}{dst_str}{promote_str}'


def usi_candidate_to_string(candidate: Candidate, index: int) -> str:
    '''候補手情報をUSI形式の解析結果文字列に変換する。
    Args:
        candidate (Candidate): 候補手情報
        index (int): 優先順位
    Returns:
        str: USI形式の解析結果文字列
    '''
    nodes = candidate.visits
    score = get_shogi_score(candidate.win_chance)
    text = f'info multipv {index} nodes {nodes} score cp {score}'

    if len(candidate.variations) > 0:
        pv = ' '.join(usi_move_to_string(s, d, p) for s, d, p in candidate.variations)
        text += f' pv {pv}'

    return text


class USIEngine(object):
    '''USIプロトコルの入力にもとづいて思考処理を実行するクラス'''

    def __init__(
        self,
        processor: Processor,
        threads: int,
        visits: int,
        playouts: int = 0,
        timelimit: float = 0,
        use_ucb1: bool = False,
        ponder: bool = False,
        resign_threshold: float = 0.0,
        resign_turn: int = 0,
        initial_turn: int = 4,
        initial_width: int = 16,
        nyugyoku_scores: Tuple[int, int] = DEFAULT_NYUGYOKU_SCORES,
        check_search_depth: int = 31,
        check_search_node: int = 10_000,
        check_node_depth: int = 2,
        client_name: str = NAME,
        client_version: str = VERSION,
        client_author: str = AUTHOR,
        reader: TextIO = sys.stdin,
        writer: TextIO = sys.stdout,
    ):
        '''GTPエンジンを作成する。
        Args:
            processor(Processor): 推論実行オブジェクト
            threads(int): 使用するスレッドの数
            visits(int): 探索の訪問回数
            playouts(int): 探索のプレイアウト回数
            timelimit(float): 思考時間の制限(秒)
            use_ucb1(bool): UCB1を使用するならTrue、PUCBを使用するならFalse
            ponder(bool): 相手の思考中に解析を継続するならTrue
            resign_threshold(float): 投了するときの勝率
            resign_turn(int): 投了するまでの最低ターン数
            initial_turn(int): ランダム着手する初期ターン数
            initial_width(int): ランダム着手の候補手の数
            nyugyoku_scores(Tuple[int,int]): 入玉宣言に必要となる点数
            check_search_depth(int): 詰み探索の深さ
            check_search_node(int): 詰み探索のノード数
            check_node_depth(int): 詰み探索を行うノードの深さ
            max_visits(int): 探索の最大訪問数
            client_name(str): クライアントの表示名
            client_version(str): クライアントの表示バージョン
            client_author(str): 表示する作者名
            reader(TextIO): 命令を入力するストリーム
            writer(TextIO): 結果を出力するストリーム
        '''
        self.player: Player | None = None
        self.processor = processor
        self.threads = threads
        self.nyugyoku_scores = nyugyoku_scores
        self.check_search_depth = check_search_depth
        self.check_search_node = check_search_node
        self.check_node_depth = check_node_depth

        self.visits = visits
        self.playouts = playouts
        self.timelimit = timelimit
        self.use_ucb1 = use_ucb1
        self.ponder = ponder

        self.resign_threshold = resign_threshold
        self.resign_turn = resign_turn

        self.initial_turn = initial_turn
        self.initial_width = initial_width

        self.client_name = client_name
        self.client_version = client_version
        self.client_author = client_author

        self.reader = reader
        self.writer = writer

        self.thread: Thread | None = None
        self.terminated = False

        self.startsfen = DEFAULT_INITIAL_SFEN
        self.moves: List[str] = []

        self.options: Dict[str, Tuple[str, str, Callable[[Any], str], Callable[[str], Any]]] = {
            'Threads': ('spin default {} min 1', 'threads', str, int),
            'CheckSearchDepth': ('spin default {} min 1', 'check_search_depth', str, int),
            'CheckSearchNode': ('spin default {} min 1', 'check_search_node', str, int),
            'CheckNodeDepth': ('spin default {} min 1', 'check_node_depth', str, int),
            'NyugyokuRule': ('combo default {} var 27 var 24', 'nyugyoku_scores',
                             lambda v: '24' if v == (31, 31) else '27',
                             lambda s: (31, 31) if s == '24' else DEFAULT_NYUGYOKU_SCORES),
            'Visits': ('spin default {} min 1', 'visits', str, int),
            'Playouts': ('spin default {} min 0', 'playouts', str, int),
            'Timelimit': ('spin default {} min 0', 'timelimit',
                          lambda v: str(int(v * 1000)), lambda s: float(s) / 1000),
            'UseUCB1': ('check default {}', 'use_ucb1',
                        lambda v: str(v).lower(), lambda s: s.lower() == 'true'),
            'Ponder': ('check default {}', 'ponder',
                       lambda v: str(v).lower(), lambda s: s.lower() == 'true'),
            'ResignThreshold': ('spin default {} min 0 max 100', 'resign_threshold',
                                lambda v: str(int(v * 100)), lambda s: float(s) / 100),
            'ResignTurn': ('spin default {} min 0', 'resign_turn', str, int),
            'InitialTurn': ('spin default {} min 0', 'initial_turn', str, int),
        }

    def run(self) -> None:
        '''USIエンジンを実行する。'''
        while True:
            try:
                # コマンドを読み込む
                command = self.reader.readline()

                # 読み込むデータが存在しない（改行文字がない）場合は終了する
                if len(command) == 0:
                    break

                # 改行文字を削除する
                command = command.strip()

                # 何も読み込めなかった場合は読み込みを継続する
                if len(command) == 0:
                    continue

                # 実行中のスレッドがあれば終了する
                if self.thread is not None:
                    self.terminated = True
                    self.thread.join()

                # ログを出力する
                LOGGER.debug('USI command: %s', command)

                # 終了命令ならループを抜ける
                if command.lower().startswith('quit'):
                    self.thread = None
                    break

                # 別スレッドでコマンドを実行する
                self.terminated = False
                self.thread = Thread(target=self._perform, args=(command,))
                self.thread.start()
            except BaseException as e:
                LOGGER.error('USI error: %s', e)
                break

    def _get_timelimit(
        self,
        remain_time: float = 0x7fffffff,
        byoyomi_time: float = 0.0,
        increment_time: float = 0.0,
    ) -> float:
        '''思考時間を取得する。
        Args:
            remain_time(float): 残り時間
            byoyomi_time(float): 秒読み時間
            increment_time(float): 加算時間
        Returns:
            float: 思考時間
        '''
        each_time = byoyomi_time + increment_time
        margin_time = 20.0 if each_time < 2.0 else 0.0
        timelimit = max(remain_time - margin_time, 0.0) * 0.1
        timelimit += max(each_time - 2.0, 0.0)

        return min(self.timelimit, timelimit)

    def _evaluate(self, visits: int, playouts: int, timelimit: float) -> List[Candidate]:
        '''盤面を評価する。
        Args:
            visits(int): 探索の訪問回数
            playouts(int): 探索のプレイアウト回数
            timelimit(float): 評価時間の目標値
        Returns:
            List[Candidate]: 候補手の一覧
        '''
        if self.player is None:
            raise ShogiException('player is not initialized')

        if len(self.moves) < self.initial_turn:
            LOGGER.debug('RandomMove: turn=%d', len(self.moves))
            return [self.player.get_random(width=self.initial_width, timelimit=timelimit)]
        else:
            LOGGER.debug(
                'Evaluate: visits=%d, playouts=%d, timelimit=%.1f, ponder=%s',
                visits, playouts, timelimit, self.ponder)
            return self.player.evaluate(
                visits=visits,
                playouts=playouts,
                timelimit=timelimit,
                use_ucb1=self.use_ucb1,
                check_node_depth=self.check_node_depth,
                ponder=self.ponder)

    def _perform(self, command: str) -> None:
        '''命令を実行する。
        Args:
            command(str): 命令
        '''
        while True:
            try:
                # 処理を実行する
                stat, message, cont = self._perform_command(command)

                # 応答を出力する
                if stat:
                    LOGGER.debug('USI response: %s', message)
                    self.writer.write(f'{message}\n')
                    self.writer.flush()
                elif len(message) > 0:
                    LOGGER.error('USI error: %s', message)

                # 実行を継続しない場合はループを抜ける
                if not cont or self.terminated:
                    break
            except BaseException as e:
                LOGGER.error('USI error: %s', e)
                break

    def _perform_command(self, command: str) -> Tuple[bool, str, bool]:
        '''コマンドを実行する。
        Args:
            command(str): コマンド文字列(引数を含む)
        Returns:
            Tuple[bool, str, bool]: (出力するならTrue, メッセージ, 実行を継続するならTrue)
        '''
        tokens = [s for s in command.split() if len(s) != 0]
        command = tokens[0].lower().replace('-', '_')
        args = tokens[1:]
        method_name = '_perform_command_{}'.format(command)

        if hasattr(self, method_name):
            try:
                return getattr(self, method_name)(args)
            except ShogiException as e:
                LOGGER.error('ShogiException: %s', e)
                return (False, str(e), False)
        else:
            LOGGER.error('Unknown command: %s', command)
            return (False, 'unknown command', False)

    def _perform_command_usi(self, args: List[str]) -> Tuple[bool, str, bool]:
        '''起動時の情報を返す。
        Args:
            args(List[str]): 引数
        Returns:
            Tuple[bool, str, bool]: (出力するならTrue, メッセージ, 実行を継続するならTrue)
        '''
        options = '\n'.join(
            f'option name {name} type {fmt.format(func(getattr(self, var)))}'
            for name, (fmt, var, func, _) in self.options.items())

        message = (
            f'id name {self.client_name} {self.client_version}\n'
            f'id author {self.client_author}\n'
            f'{options}\n'
            'usiok')
        return (True, message, False)

    def _perform_command_setoption(self, args: List[str]) -> Tuple[bool, str, bool]:
        '''オプション設定には対応していないので、このコマンドは無視する。
        Args:
            args(List[str]): 引数
        Returns:
            Tuple[bool, str, bool]: (出力するならTrue, メッセージ, 実行を継続するならTrue)
        '''
        if len(args) < 4:
            return (False, 'syntax error', False)

        _, name, _, value = args[:4]

        if name not in self.options:
            return (False, f'unknown option: {name}', False)

        _, var, _, func = self.options[name]
        setattr(self, var, func(value))

        return (False, '', False)

    def _perform_command_isready(self, args: List[str]) -> Tuple[bool, str, bool]:
        '''準備完了を返す。
        Args:
            args(List[str]): 引数
        Returns:
            Tuple[bool, str, bool]: (出力するならTrue, メッセージ, 実行を継続するならTrue)
        '''
        # プレイヤオブジェクトが作成されていない場合は作成する
        if self.player is None:
            self.player = Player(
                processor=self.processor,
                threads=self.threads,
                nyugyoku_scores=self.nyugyoku_scores,
                check_search_depth=self.check_search_depth,
                check_search_node=self.check_search_node)

        return (True, 'readyok', False)

    def _perform_command_usinewgame(self, args: List[str]) -> Tuple[bool, str, bool]:
        '''新しいゲームを開始する。
        Args:
            args(List[str]): 引数
        Returns:
            Tuple[bool, str, bool]: (出力するならTrue, メッセージ, 実行を継続するならTrue)
        '''
        # プレイヤオブジェクトが作成されていることを確認する
        if self.player is None:
            raise ShogiException('player is not initialized')

        # ゲームの状態を初期化する
        self.startsfen = DEFAULT_INITIAL_SFEN
        self.moves.clear()

        # プレイヤオブジェクトを初期化する
        self.player.initialize(self.startsfen)

        return (False, '', False)

    def _perform_command_position(self, args: List[str]) -> Tuple[bool, str, bool]:
        '''局面を設定する。
        Args:
            args(List[str]): 引数
        Returns:
            Tuple[bool, str, bool]: (出力するならTrue, メッセージ, 実行を継続するならTrue)
        '''
        # プレイヤオブジェクトが作成されていることを確認する
        if self.player is None:
            raise ShogiException('player is not initialized')

        # 開始局面を取得する
        index = 0

        if index < len(args) and args[index].lower() == 'startpos':
            sfen = DEFAULT_INITIAL_SFEN
            index += 1
        elif index + 1 < len(args) and args[index].lower() == 'sfen':
            sfen = ' '.join(args[index + 1:index + 5])
            index += 5
        else:
            return (False, 'syntax error', False)

        # 着手一覧を取得する
        if index < len(args) and args[index].lower() == 'moves':
            moves = args[index + 1:]
        else:
            moves = []

        # 開始局面と着手履歴が一致する場合は必要となる着手のみ残す
        if sfen == self.startsfen and moves[:len(self.moves)] == self.moves:
            moves = moves[len(self.moves):]
        # 一致しない場合は局面を初期化する
        else:
            self.player.initialize(sfen)
            self.startsfen = sfen
            self.moves.clear()

        # 着手を実行する
        for move in moves:
            src, dst, promote = usi_string_to_move(move)
            self.player.play(src, dst, promote)
            self.moves.append(move)

        # ログを出力する
        if LOGGER.isEnabledFor(logging.DEBUG):
            LOGGER.debug(
                'Position: sfen=%s, moves=%s\n%s',
                sfen, ','.join(moves), self.player.get_board())

        return (False, '', False)

    def _perform_command_go(
        self,
        args: List[str],
        analyze: bool = True,
    ) -> Tuple[bool, str, bool]:
        '''思考を開始する。
        Args:
            args(List[str]): 引数リスト
            analyze(bool): 解析結果も出力するならTrue
        Returns:
            Tuple[bool, str, bool]: (出力するならTrue, メッセージ, 実行を継続するならTrue)
        '''
        # プレイヤオブジェクトが作成されていることを確認する
        if self.player is None:
            raise ShogiException('player is not initialized')

        # 引数を解析する
        index = 0
        ponder = False
        btime = -1.0
        wtime = -1.0
        byoyomi = 0.0
        binc = 0.0
        winc = 0.0

        while index < len(args):
            if args[index].lower() == 'ponder':
                ponder = True
                index += 1
            elif args[index].lower() == 'infinite':
                ponder = True
                index += 1
            elif index + 1 < len(args) and args[index].lower() == 'btime':
                btime = float(args[index + 1]) / 1000
                index += 2
            elif index + 1 < len(args) and args[index].lower() == 'wtime':
                wtime = float(args[index + 1]) / 1000
                index += 2
            elif index + 1 < len(args) and args[index].lower() == 'byoyomi':
                byoyomi = float(args[index + 1]) / 1000
                index += 2
            elif index + 1 < len(args) and args[index].lower() == 'binc':
                binc = float(args[index + 1]) / 1000
                index += 2
            elif index + 1 < len(args) and args[index].lower() == 'winc':
                winc = float(args[index + 1]) / 1000
                index += 2
            elif index + 1 < len(args) and args[index].lower() == 'mate':
                if args[index + 1].lower() != 'infinite':
                    btime = float(args[index + 1]) / 1000
                    wtime = float(args[index + 1]) / 1000
                index += 2

        # 制限時間を計算する
        # 残り時間が設定されいない場合はデフォルトの制限時間を使用する
        # 引数でponderが指定されている場合は制限時間を0.5秒にする
        if ponder:
            timelimit = 0.5
        elif self.player.get_color() == COLOR_BLACK and btime >= 0:
            timelimit = self._get_timelimit(btime, byoyomi, binc)
        elif self.player.get_color() == COLOR_WHITE and wtime >= 0:
            timelimit = self._get_timelimit(wtime, byoyomi, winc)
        else:
            timelimit = self._get_timelimit()

        # ゲームの状態を確認する
        board = self.player.get_board()

        if len(board.get_legal_moves()) == 0:
            LOGGER.debug(
                'GameEnd: winner=%s',
                get_color_name(get_opposite_color(self.player.get_color())))
            return (True, 'bestmove resign', False)

        if board.is_nyugyoku():
            LOGGER.debug(
                'GameEnd: winner=%s',
                get_color_name(self.player.get_color()))
            return (True, 'bestmove win', False)

        # 着手を計算する
        # 引数でponderが指定されている場合は訪問回数を100,000,000に設定する
        visits = 100_000_000 if ponder else self.visits
        candidates = self._evaluate(visits, self.playouts, timelimit)

        # 候補手がない場合は例外を発生させる（探索設定を間違っている場合のみ発生する）
        if candidates[0].src == -1:
            raise ShogiException('no candidates')

        # 出力を作成する
        results: List[str] = []

        # 解析結果の文字列を作成する
        if analyze:
            results.extend(
                usi_candidate_to_string(candidate, i + 1)
                for i, candidate in enumerate(candidates))

        # ponderならbestmoveを返さない
        if ponder:
            return (True, '\n'.join(results), True)

        # 勝率が閾値以下場合は投了と判定する
        if (not ponder
                and len(self.moves) > self.resign_turn
                and candidates[0].win_chance < self.resign_threshold):
            LOGGER.debug('Resign: win_chance=%.2f', candidates[0].win_chance)
            return (True, 'bestmove resign', False)

        # 着手を追加する
        move = usi_move_to_string(
            candidates[0].src, candidates[0].dst, candidates[0].promote)

        # 結果にbestmoveを追加する
        results.append(f'bestmove {move}')

        # ログを出力する
        if LOGGER.isEnabledFor(logging.DEBUG):
            board = Board(self.startsfen)
            board.copy_from(self.player.get_board())
            board.play(candidates[0].src, candidates[0].dst, candidates[0].promote)

            LOGGER.debug(
                'Bestmove: src=%s, dst=%s, promote=%s\n%s',
                candidates[0].src, candidates[0].dst, candidates[0].promote, board)

        return (True, '\n'.join(results), False)

    def _perform_command_stop(self, args: List[str]) -> Tuple[bool, str, bool]:
        '''思考を中断する。
        Args:
            args(List[str]): 引数リスト
        Returns:
            Tuple[bool, str, bool]: (出力するならTrue, メッセージ, 実行を継続するならTrue)
        '''
        return self._perform_command_go(['btime', '0', 'wtime', '0'], analyze=False)

    def _perform_command_ponderhit(self, args: List[str]) -> Tuple[bool, str, bool]:
        '''ponderhitコマンドを実行する。
        Args:
            args(List[str]): 引数リスト
        Returns:
            Tuple[bool, str, bool]: (出力するならTrue, メッセージ, 実行を継続するならTrue)
        '''
        return self._perform_command_go(['btime', '0', 'wtime', '0'], analyze=False)

    def _perform_command_gameover(self, args: List[str]) -> Tuple[bool, str, bool]:
        '''ゲーム終了を通知する。
        Args:
            args(List[str]): 引数リスト
        Returns:
            Tuple[bool, str, bool]: (出力するならTrue, メッセージ, 実行を継続するならTrue)
        '''
        # プレイヤオブジェクトが作成されていることを確認する
        if self.player is None:
            raise ShogiException('player is not initialized')

        # プレイヤオブジェクトを初期化する
        self.player.initialize()

        return (False, '', False)

    def _perform_command_showboard(self, args: List[str]) -> Tuple[bool, str, bool]:
        '''現在の局面を返す。
        Args:
            args(List[str]): 引数
        Returns:
            Tuple[bool, str, bool]: (出力するならTrue, メッセージ, 実行を継続するならTrue)
        '''
        # プレイヤオブジェクトが作成されていることを確認する
        if self.player is None:
            raise ShogiException('player is not initialized')

        # 現在の局面を返す
        return (True, str(self.player.get_board()), False)

    def _perform_command_play(self, args: List[str]) -> Tuple[bool, str, bool]:
        '''playコマンドを実行する。
        Args:
            args(List[str]): 引数リスト
        Returns:
            Tuple[bool, str, bool]: (出力するならTrue, メッセージ, 実行を継続するならTrue)
        '''
        # プレイヤオブジェクトが作成されていることを確認する
        if self.player is None:
            raise ShogiException('player is not initialized')

        # 引数を確認する
        if len(args) < 1:
            return (False, 'syntax error', False)

        # 着手を取得する
        move = usi_string_to_move(args[0])

        # 着手を実行する
        try:
            self.player.play(*move)
            self.moves.append(args[0])
        except ShogiException:
            return (False, 'illegal move', False)

        # ログを出力する
        if LOGGER.isEnabledFor(logging.DEBUG):
            LOGGER.debug(
                'Play: src=%s, dst=%s, promote=%s\n%s',
                move[0], move[1], move[2], self.player.get_board())

        return (False, '', False)

    def _perform_command_genmove(self, args: List[str]) -> Tuple[bool, str, bool]:
        '''genmoveコマンドを実行する。
        Args:
            args(List[str]): 引数リスト
        Returns:
            Tuple[bool, str, bool]: (出力するならTrue, メッセージ, 実行を継続するならTrue)
        '''
        # プレイヤオブジェクトが作成されていることを確認する
        if self.player is None:
            raise ShogiException('player is not initialized')

        # 着手を計算する
        _, message, _ = self._perform_command_go([], analyze=False)
        move = message.split()[1]
        src, dst, promote = usi_string_to_move(move)

        # 盤面を進める
        self.player.play(src, dst, promote)
        self.moves.append(move)

        # 着手を返す
        return (True, message, False)
