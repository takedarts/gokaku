import argparse
from typing import Tuple

from deepshogi.board import Board
from deepshogi.config import (BOARD_SIZE, COLOR_BLACK, PIECE_BLACK_BEGIN,
                              PIECE_BLACK_END, PIECE_HAND_BEGIN,
                              PIECE_WHITE_BEGIN, PIECE_WHITE_END)
from deepshogi.pnsearch import PnSearch


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description='Checkmate Solver using Proof-Number Search')
    parser.add_argument('sfen', type=str, help='SFEN string representing the game state')
    parser.add_argument(
        '--depth', type=int, default=31, help='Search depth for the PN search')
    parser.add_argument(
        '--nodes', type=int, default=100000, help='Maximum number of nodes for the PN search')
    parser.add_argument(
        '--japanese', action='store_true', help='Print moves in Japanese notation')
    return parser.parse_args()


def get_move_name_suffix(
    src: Tuple[int, int],
    dst: Tuple[int, int],
    promote: bool,
    color: int,
) -> Tuple[str, str, str]:
    '''Get identification characters when there are multiple legal moves to the same position with the same piece.
    Args:
        src (Tuple[int, int]): Source position
        dst (Tuple[int, int]): Destination position
        promote (bool): Whether to promote
        color (int): Side to move
    Returns:
        Tuple[str, str, str]: Strings representing move type, position type, and promotion type
    '''
    if src[0] == BOARD_SIZE:
        return ('打', '', '')

    if color == COLOR_BLACK:
        diff = (dst[0] - src[0], dst[1] - src[1])
    else:
        diff = (src[0] - dst[0], src[1] - dst[1])

    move_name = '上' if diff[1] < 0 else '引' if diff[1] > 0 else '寄'
    pos_name = '右' if diff[0] < 0 else '左' if diff[0] > 0 else '直'
    promote_name = '成' if promote else '不成'

    return (move_name, pos_name, promote_name)


def get_move_text_in_japanese(
        move: Tuple[Tuple[int, int], Tuple[int, int], bool],
        prev: Tuple[int, int],
        board: Board,
) -> str:
    '''Get the content of a move in Japanese notation.
    Args:
        move (Tuple[Tuple[int, int], Tuple[int, int], bool]): Move (source position, destination position, promotion flag)
        prev (Tuple[int, int]): Previous move's destination position
        board (Board): Board object to get piece information
    Returns:
        str: String representing the move in Japanese notation
    '''
    src, dst, promote = move
    color = board.get_color()

    # Get the piece type
    if (src[0] == BOARD_SIZE):
        piece = src[1] - PIECE_HAND_BEGIN
    else:
        piece = board.get_piece(src)

        if PIECE_BLACK_BEGIN <= piece < PIECE_BLACK_END:
            piece -= PIECE_BLACK_BEGIN
        elif PIECE_WHITE_BEGIN <= piece < PIECE_WHITE_END:
            piece -= PIECE_WHITE_BEGIN
        else:
            raise ValueError(f'Invalid piece index: {piece}')

    # Create the basic move notation
    piece_name = [
        '歩', '香', '桂', '銀', '角', '飛', '金', '玉', 'と', '成香', '成桂', '成銀', '馬', '竜'][piece]

    if dst == prev:
        x_name = '同'
        y_name = ''
    else:
        x_name = ['１', '２', '３', '４', '５', '６', '７', '８', '９'][dst[0]]
        y_name = ['一', '二', '三', '四', '五', '六', '七', '八', '九'][dst[1]]

    move_text = f'{x_name}{y_name}{piece_name}'

    # Check if there are legal moves to the same position with the same piece
    legal_moves = board.get_legal_moves(remove_unpromote=False)
    duplicate_moves = []

    for legal_move in legal_moves:
        if legal_move[0][0] == BOARD_SIZE or legal_move[1] != dst:
            continue

        move_piece = board.get_piece(legal_move[0])

        if PIECE_BLACK_BEGIN <= move_piece < PIECE_BLACK_END:
            move_piece -= PIECE_BLACK_BEGIN
        elif PIECE_WHITE_BEGIN <= move_piece < PIECE_WHITE_END:
            move_piece -= PIECE_WHITE_BEGIN
        else:
            continue

        if move_piece == piece and (legal_move[0] != src or legal_move[2] != promote):
            duplicate_moves.append(legal_move)

    # Add identification characters if there are duplicate moves
    if len(duplicate_moves) > 0:
        suffixes = get_move_name_suffix(src, dst, promote, color)
        duplicate_suffixes = [get_move_name_suffix(*m, color) for m in duplicate_moves]

        for i in range(3):
            if len(duplicate_moves) == 0:
                break

            duplicate_count = len(duplicate_suffixes)
            duplicate_suffixes = [s for s in duplicate_suffixes if s[i] == suffixes[i]]

            if len(duplicate_suffixes) < duplicate_count:
                move_text += suffixes[i]

    # Add the promotion character if it's a promotion move and "成" hasn't been added yet
    if promote and '成' not in move_text:
        move_text += '成'

    return move_text


def get_move_text(move: Tuple[Tuple[int, int], Tuple[int, int], bool]) -> str:
    '''Get the content of a move in standard notation.
    The specified move is tupled as (source position, destination position, promotion flag).
    Args:
        move (Tuple[Tuple[int, int], Tuple[int, int], bool]): Move
    Returns:
        str: String representing the move in standard notation
    '''
    src, dst, promote = move

    def pos_to_text(pos: Tuple[int, int]) -> str:
        x = ['1', '2', '3', '4', '5', '6', '7', '8', '9'][pos[0]]
        y = ['a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i'][pos[1]]
        return f'{x}{y}'

    def piece_to_text(piece: int) -> str:
        return ['P', 'L', 'N', 'S', 'B', 'R', 'G'][piece]

    # In case of dropping a piece onto the board
    if src[0] == BOARD_SIZE:
        return f'{piece_to_text(src[1])}*{pos_to_text(dst)}'
    # In case of moving a piece on the board
    else:
        return f'{pos_to_text(src)}{pos_to_text(dst)}{"+" if promote else ""}'


def main() -> None:
    args = parse_args()

    # Create the board from SFEN string
    board = Board(args.sfen)

    # Execute PN search to get checkmate moves
    search = PnSearch(nodes=args.nodes)
    moves = search.get_checkmate_moves(board, args.depth)

    # If no checkmate moves are found, display a message and exit
    if len(moves) == 0:
        if args.japanese:
            print('詰みが見つかりませんでした。')
        else:
            print('No checkmate found.')
        return

    # Display the checkmate moves
    prev = (-1, -1)

    if args.japanese:
        print(f'{len(moves)}手詰めの手が見つかりました。')
    else:
        print(f'Found checkmate move: length={len(moves)}')

    for move in moves:
        if args.japanese:
            print(get_move_text_in_japanese(move, prev, board))
            board.play(*move)
            prev = move[1]
        else:
            print(get_move_text(move))


if __name__ == '__main__':
    main()
