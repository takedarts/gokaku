################################################################
# 全体の設定
################################################################
# プログラム名
NAME = 'Gokaku'
# バージョン番号
VERSION = '2.1'
# 作者名
AUTHOR = 'Atsushi Takeda'

################################################################
# 定数値 (手番の値はcshogiと異なる)
################################################################
# 盤面の大きさ
BOARD_SIZE = 9

# 先手番の値
COLOR_BLACK = 1
# 後手番の値
COLOR_WHITE = -1
# 先手番でも後手番でもない値（引き分け等に使用）
COLOR_NONE = 0

# 入玉ルール - 27点法
NYUGYOKU_RULE_27 = 0
# 入玉ルール - 24点法
NYUGYOKU_RULE_24 = 1

# 終局理由 - なし
RESULT_NONE = 0
# 終局理由 - 詰み
RESULT_TSUMI = 1
# 終局理由 - 入玉
RESULT_NYUGYOKU = 2
# 終局理由 - 千日手
RESULT_SENNICHITE = 3
# 終局理由 - 最長手数
RESULT_MAX_MOVES = 4

# 勝率から将棋用点数に変換する際のスケール
SHOGI_SCORE_SCALE = 600


def get_opposite_color(color: int) -> int:
    '''指定された手番の相手の手番を取得する。
    Args:
        color (int): 手番
    Returns:
        int: 相手の手番
    '''
    return color * -1


def get_color_name(color: int) -> str:
    '''手番を表す文字列を取得する。
    Args:
        color (int): 石の色
    Returns:
        str: 石の色を表す文字列
    '''
    if color == COLOR_BLACK:
        return 'black'
    elif color == COLOR_WHITE:
        return 'white'
    else:
        return 'none'


def get_result_name(result: int) -> str:
    '''終局理由の名前を取得する。'''
    if result == RESULT_NONE:
        return 'none'
    elif result == RESULT_TSUMI:
        return 'tsumi'
    elif result == RESULT_NYUGYOKU:
        return 'nyugyoku'
    elif result == RESULT_SENNICHITE:
        return 'sennichite'
    elif result == RESULT_MAX_MOVES:
        return 'max_moves'
    else:
        return 'unknown'


def get_shogi_score(win_chance: float) -> int:
    '''盤面評価値から将棋用の点数に変換する。
    Args:
        win_chance (float): 予想勝率
    Returns:
        int: 将棋用の点数
    '''
    import math

    win_chance = min(max(win_chance, 0.0 + 1e-9), 1.0 - 1e-9)
    score = -600 * math.log(1 / win_chance - 1)
    score = max(min(score, 9999), -9999)

    return round(score)


################################################################
# 駒番号（cshogiの駒番号と対応）
################################################################
PIECE_EMPTY = 0
PIECE_PROMOTE = 8

PIECE_BLACK_BEGIN = 1
PIECE_BLACK_PAWN = 1
PIECE_BLACK_LANCE = 2
PIECE_BLACK_KNIGHT = 3
PIECE_BLACK_SILVER = 4
PIECE_BLACK_BISHOP = 5
PIECE_BLACK_ROOK = 6
PIECE_BLACK_GOLD = 7
PIECE_BLACK_KING = 8
PIECE_BLACK_PRO_PAWN = 9
PIECE_BLACK_PRO_LANCE = 10
PIECE_BLACK_PRO_KNIGHT = 11
PIECE_BLACK_PRO_SILVER = 12
PIECE_BLACK_HORSE = 13
PIECE_BLACK_DRAGON = 14
PIECE_BLACK_END = 15

PIECE_WHITE_BEGIN = 17
PIECE_WHITE_PAWN = 17
PIECE_WHITE_LANCE = 18
PIECE_WHITE_KNIGHT = 19
PIECE_WHITE_SILVER = 20
PIECE_WHITE_BISHOP = 21
PIECE_WHITE_ROOK = 22
PIECE_WHITE_GOLD = 23
PIECE_WHITE_KING = 24
PIECE_WHITE_PRO_PAWN = 25
PIECE_WHITE_PRO_LANCE = 26
PIECE_WHITE_PRO_KNIGHT = 27
PIECE_WHITE_PRO_SILVER = 28
PIECE_WHITE_HORSE = 29
PIECE_WHITE_DRAGON = 30
PIECE_WHITE_END = 31

PIECE_HAND_BEGIN = 0
PIECE_HAND_PAWN = 0
PIECE_HAND_LANCE = 1
PIECE_HAND_KNIGHT = 2
PIECE_HAND_SILVER = 3
PIECE_HAND_BISHOP = 4
PIECE_HAND_ROOK = 5
PIECE_HAND_GOLD = 6
PIECE_HAND_END = 7

################################################################
# モデルの設定値
################################################################
# モデルに入力する盤面特徴量の数
MODEL_FEATURES = 84
# モデルに入力するゲーム特徴量の数
MODEL_INFOS = 80
# モデルが出力するポリシーの層数
MODEL_POLICIES = 82
# モデルが出力する盤面予測値の数
MODEL_PREDICTIONS = MODEL_POLICIES * 2 + 1
# モデルが出力するゲーム予測値の数
MODEL_VALUES = 8

# モデルに入力するデータの大きさ
MODEL_INPUT_SIZE = MODEL_FEATURES * BOARD_SIZE * BOARD_SIZE + MODEL_INFOS
# モデルが出力するデータの大きさ
MODEL_OUTPUT_SIZE = MODEL_PREDICTIONS * BOARD_SIZE * BOARD_SIZE + MODEL_VALUES

################################################################
# デフォルト値
################################################################
# デフォルトの初期盤面
DEFAULT_INITIAL_SFEN = 'lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1'
# デフォルトの詰み探索の深さ
DEFAULT_CHECK_SEARCH_DEPTH = 31
# デフォルトの詰み探索のノード数
DEFAULT_CHECK_SEARCH_NODE = 10_000
# デフォルトの同一局面の繰り返し回数
DEFAULT_ALLOWED_REPEATS = 3
# デフォルトの入玉宣言に必要となる点数
DEFAULT_NYUGYOKU_SCORES = (28, 27)
# デフォルトの引き分け手数
DEFAULT_DRAW_STEPS = 512

################################################################
# ロギングの設定
################################################################
# ログの形式
LOGGING_FORMAT = '%(asctime)s [%(levelname)-5.5s] %(message)s (%(module)s.%(funcName)s:%(lineno)s)'
# 時刻表示の形式
LOGGING_DATE_FORMAT = '%Y-%m-%d %H:%M:%S'
