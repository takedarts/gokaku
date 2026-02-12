################################################################
# Global settings
################################################################
# Program name
NAME = 'Gokaku'
# Version number
VERSION = '2.2'
# Author name
AUTHOR = 'Atsushi Takeda'

################################################################
# Constant values (the value for side to move is different from cshogi)
################################################################
# Board size
BOARD_SIZE = 9

# Value for black (first player)
COLOR_BLACK = 1
# Value for white (second player)
COLOR_WHITE = -1
# Value for neither black nor white (used for draws, etc.)
COLOR_NONE = 0

# Nyugyoku rule - 27 points method
NYUGYOKU_RULE_27 = 0
# Nyugyoku rule - 24 points method
NYUGYOKU_RULE_24 = 1

# End reason - none
RESULT_NONE = 0
# End reason - checkmate
RESULT_TSUMI = 1
# End reason - nyugyoku
RESULT_NYUGYOKU = 2
# End reason - repetition
RESULT_SENNICHITE = 3
# End reason - maximum moves
RESULT_MAX_MOVES = 4

# Scale for converting win rate to shogi score
SHOGI_SCORE_SCALE = 600


def get_opposite_color(color: int) -> int:
    '''Get the opposite side to move for the specified color.
    Args:
        color (int): Side to move
    Returns:
        int: Opponent's side to move
    '''
    return color * -1


def get_color_name(color: int) -> str:
    '''Get the string representing the side to move.
    Args:
        color (int): Piece color
    Returns:
        str: String representing the piece color
    '''
    if color == COLOR_BLACK:
        return 'black'
    elif color == COLOR_WHITE:
        return 'white'
    else:
        return 'none'


def get_result_name(result: int) -> str:
    '''Get the name of the end reason.'''
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
    '''Convert board evaluation value to shogi score.
    Args:
        win_chance (float): Expected win rate
    Returns:
        int: Shogi score
    '''
    import math

    win_chance = min(max(win_chance, 0.0 + 1e-9), 1.0 - 1e-9)
    score = -600 * math.log(1 / win_chance - 1)
    score = max(min(score, 9999), -9999)

    return round(score)


################################################################
# Piece numbers (corresponds to cshogi piece numbers)
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
# Model settings
################################################################
# Number of board features input to the model
MODEL_FEATURES = 84
# Number of game features input to the model
MODEL_INFOS = 80
# Number of policy layers output by the model
MODEL_POLICIES = 82
# Number of board predictions output by the model
MODEL_PREDICTIONS = MODEL_POLICIES * 2 + 1
# Number of game predictions output by the model
MODEL_VALUES = 8

# Size of data input to the model
MODEL_INPUT_SIZE = MODEL_FEATURES * BOARD_SIZE * BOARD_SIZE + MODEL_INFOS
# Size of data input to the model when embedded as int32
MODEL_INPUT_PACK_SIZE = (MODEL_INPUT_SIZE + 31) // 32 + 3
# Size of data output by the model
MODEL_OUTPUT_SIZE = MODEL_PREDICTIONS * BOARD_SIZE * BOARD_SIZE + MODEL_VALUES

################################################################
# Search algorithm settings
################################################################
# Search algorithm: UCB
SEARCH_UCB = 0
# Search algorithm: PUCB
SEARCH_PUCB = 1

################################################################
# Default values
################################################################
# Default initial board
DEFAULT_INITIAL_SFEN = 'lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1'
# Default depth for checkmate search
DEFAULT_CHECK_SEARCH_DEPTH = 31
# Default number of nodes for checkmate search
DEFAULT_CHECK_SEARCH_NODE = 10_000
# Default node depth for checkmate search
DEFAULT_CHECK_NODE_DEPTH = 4
# Default number of allowed repeats of the same position
DEFAULT_ALLOWED_REPEATS = 3
# Default points required for nyugyoku declaration
DEFAULT_NYUGYOKU_SCORES = (28, 27)
# Default number of moves for a draw
DEFAULT_DRAW_TURN = 512
# Default maximum number of visits for search
DEFAULT_MAX_VISITS = 1_000_000
# Constant multiplied to UCB upper confidence bound
DEFAULT_UCB_CONSTANT = 1.4
# Initial value applied to PUCB upper confidence bound
DEFAULT_PUCB_CONSTANT_INIT = 1.2
# Base value applied to PUCB upper confidence bound
DEFAULT_PUCB_CONSTANT_BASE = 18200.0

################################################################
# Logging settings
################################################################
# Log format
LOGGING_FORMAT = '%(asctime)s [%(levelname)-5.5s] %(message)s (%(module)s.%(funcName)s:%(lineno)s)'
# Date format for log timestamps
LOGGING_DATE_FORMAT = '%Y-%m-%d %H:%M:%S'
