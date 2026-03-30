import itertools
import struct

from .config import BOARD_SIZE
from .exception import ShogiException

BOARD_HUFFMAN_CODES = [
    ('P', 0b0001, 4),  # Pawn(Black)
    ('L', 0b000011, 6),  # Lance(Black)
    ('N', 0b001011, 6),  # Night(Black)
    ('S', 0b000111, 6),  # Silver(Black)
    ('B', 0b00011111, 8),  # Bishop(Black)
    ('R', 0b00111111, 8),  # Rook(Black)
    ('G', 0b001111, 6),  # Gold(Black)
    ('+P', 0b0101, 4),  # P-Pawn(Black)
    ('+L', 0b010011, 6),  # P-Lance(Black)
    ('+N', 0b011011, 6),  # P-Night(Black)
    ('+S', 0b010111, 6),  # P-Silver(Black)
    ('+B', 0b01011111, 8),  # Horse(Black)
    ('+R', 0b01111111, 8),  # Dragon(Black)
    ('p', 0b1001, 4),  # Pawn(White)
    ('l', 0b100011, 6),  # Lance(White)
    ('n', 0b101011, 6),  # Night(White)
    ('s', 0b100111, 6),  # Silver(White)
    ('b', 0b10011111, 8),  # Bishop(White)
    ('r', 0b10111111, 8),  # Rook(White)
    ('g', 0b101111, 6),  # Gold(White)
    ('+p', 0b1101, 4),  # P-Pawn(White)
    ('+l', 0b110011, 6),  # P-Lance(White)
    ('+n', 0b111011, 6),  # P-Night(White)
    ('+s', 0b110111, 6),  # P-Silver(White)
    ('+b', 0b11011111, 8),  # Horse(White)
    ('+r', 0b11111111, 8),  # Dragon(White)
]

HAND_HUFFMAN_CODES = [
    ('P', 0b000, 3),  # Pawn(Black),
    ('L', 0b00001, 5),  # Lance(Black),
    ('N', 0b00101, 5),  # Knight(Black),
    ('S', 0b00011, 5),  # Silver(Black),
    ('G', 0b00111, 5),  # Gold(Black),
    ('B', 0b0001111, 7),  # Bishop(Black),
    ('R', 0b0011111, 7),  # Rook(Black),
    ('p', 0b100, 3),  # Pawn(White),
    ('l', 0b10001, 5),  # Lance(White),
    ('n', 0b10101, 5),  # Knight(White),
    ('s', 0b10011, 5),  # Silver(White),
    ('g', 0b10111, 5),  # Gold(White),
    ('b', 0b1001111, 7),  # Bishop(White),
    ('r', 0b1011111, 7),  # Rook(White),
]

BOX_HUFFMAN_CODES = [
    ('P', 0b010, 3),  # Pawn(Black),
    ('L', 0b01001, 5),  # Lance(Black),
    ('N', 0b01101, 5),  # Knight(Black),
    ('S', 0b01011, 5),  # Silver(Black),
    ('G', 0b11011, 5),  # Gold(Black),
    ('B', 0b101111, 7),  # Bishop(Black),
    ('R', 0b111111, 7),  # Rook(Black),
]

SFEN_HAND_NAMES = list('RBGSNLPrbgsnlp')


def convert_sfen_to_psfen(sfen: str) -> bytes:
    '''Convert SFEN format board description to Packed Sfen format.
    Args:
        sfen (str): Board description in SFEN format.
    Returns:
        bytes: Board description in Packed Sfen format.
    '''
    psfen = 1 << 256

    # Split SFEN format into components
    board_sfen, color_sfen, hand_sfen, _ = sfen.split()

    # Initialize dictionary to count remaining pieces
    piece_counts = {'P': 18, 'L': 4, 'N': 4, 'S': 4, 'G': 4, 'B': 2, 'R': 2, 'K': 2}

    # Get piece placements on the board
    cells = [[''] * BOARD_SIZE for _ in range(BOARD_SIZE)]
    promote = ''
    pos_x = 0
    pos_y = 0

    for c in board_sfen:
        if c == '/':
            pos_y += 1
            pos_x = 0
        elif c.isdigit():
            pos_x += int(c)
        elif c == '+':
            promote = '+'
        else:
            piece_counts[c.upper()] -= 1
            cells[pos_y][pos_x] = promote + c
            promote = ''
            pos_x += 1

    # Get hand piece information
    hand_counts = {
        'P': 0, 'L': 0, 'N': 0, 'S': 0, 'G': 0, 'B': 0, 'R': 0,
        'p': 0, 'l': 0, 'n': 0, 's': 0, 'g': 0, 'b': 0, 'r': 0,
    }
    hand_num = 1

    for c in hand_sfen:
        if c == '-':
            continue
        elif c.isdigit():
            hand_num = int(c)
        else:
            piece_counts[c.upper()] -= hand_num
            hand_counts[c] += hand_num
            hand_num = 1

    # Set turn
    if color_sfen == 'w':
        psfen |= 1

    # Set king positions
    black_king_pos = 0
    white_king_pos = 0

    for x, y in itertools.product(range(BOARD_SIZE), repeat=2):
        if cells[y][BOARD_SIZE - 1 - x] == 'K':
            black_king_pos = y + x * BOARD_SIZE
        elif cells[y][BOARD_SIZE - 1 - x] == 'k':
            white_king_pos = y + x * BOARD_SIZE

    psfen |= black_king_pos << 1
    psfen |= white_king_pos << 8

    # Set board pieces
    board_huffman_table = {k: (c, n) for k, c, n in BOARD_HUFFMAN_CODES}
    offset = 15

    for x, y in itertools.product(range(BOARD_SIZE), repeat=2):
        key = cells[y][BOARD_SIZE - 1 - x]

        if key == 'K' or key == 'k':
            continue
        elif key == '':
            offset += 1
        elif key not in board_huffman_table:
            raise ShogiException(f'Unsupported piece: {key}')
        else:
            code, num_of_bits = board_huffman_table[key]
            psfen |= code << offset
            offset += num_of_bits

    # Set hand pieces
    for key, code, num_of_bits in HAND_HUFFMAN_CODES:
        count = hand_counts[key]
        for _ in range(count):
            psfen |= code << offset
            offset += num_of_bits

    # Set piece box
    for key, code, num_of_bits in BOX_HUFFMAN_CODES:
        count = piece_counts[key.upper()]
        for _ in range(count):
            psfen |= code << offset
            offset += num_of_bits

    # Convert to Packed Sfen format bytes and return
    return struct.pack(
        '<QQQQ',
        (psfen >> 0) & 0xffffffffffffffff,
        (psfen >> 64) & 0xffffffffffffffff,
        (psfen >> 128) & 0xffffffffffffffff,
        (psfen >> 192) & 0xffffffffffffffff)


def convert_psfen_to_sfen(psfen: bytes) -> str:
    '''Convert Packed Sfen format board description to SFEN format.
    Args:
        psfen (bytes): Board description in Packed Sfen format.
    Returns:
        str: Board description in SFEN format.
    '''
    psfen_value = int.from_bytes(psfen, 'little')

    # Get turn
    if psfen_value & 1:
        color_sfen = 'w'
    else:
        color_sfen = 'b'

    # Get king positions
    black_king_pos = (psfen_value >> 1) & 0x7f
    white_king_pos = (psfen_value >> 8) & 0x7f

    # Get board piece placements
    cells = [[''] * BOARD_SIZE for _ in range(BOARD_SIZE)]
    offset = 15

    for pos, key in ((black_king_pos, 'K'), (white_king_pos, 'k')):
        y = pos % BOARD_SIZE
        x = BOARD_SIZE - 1 - (pos // BOARD_SIZE)
        cells[y][x] = key

    for x, y in itertools.product(range(BOARD_SIZE), repeat=2):
        if cells[y][BOARD_SIZE - 1 - x] != '':
            continue

        code_found = False

        for key, code, num_of_bits in BOARD_HUFFMAN_CODES:
            mask = (1 << num_of_bits) - 1
            bits = (psfen_value >> offset) & mask

            if bits == code:
                cells[y][BOARD_SIZE - 1 - x] = key
                offset += num_of_bits
                code_found = True
                break

        if not code_found:
            offset += 1

    # Get hand pieces
    hand_counts = {
        'P': 0, 'L': 0, 'N': 0, 'S': 0, 'G': 0, 'B': 0, 'R': 0,
        'p': 0, 'l': 0, 'n': 0, 's': 0, 'g': 0, 'b': 0, 'r': 0,
    }

    while offset < 256:
        code_found = False

        for key, code, num_of_bits in HAND_HUFFMAN_CODES:
            mask = (1 << num_of_bits) - 1
            bits = (psfen_value >> offset) & mask

            if bits == code:
                hand_counts[key] += 1
                offset += num_of_bits
                code_found = True
                break

        if not code_found:
            break

    # Build SFEN format of board description
    def get_line_sfen(line: list[str]) -> str:
        sfen_chars = []
        empty_count = 0

        for c in line:
            if c == '':
                empty_count += 1
            else:
                if empty_count > 0:
                    sfen_chars.append(str(empty_count))
                    empty_count = 0
                sfen_chars.append(c)

        if empty_count > 0:
            sfen_chars.append(str(empty_count))

        return ''.join(sfen_chars)

    board_sfen = '/'.join(get_line_sfen(cells[y]) for y in range(BOARD_SIZE))

    # Build SFEN format of hand pieces
    hand_sfen_chars = []

    for key in SFEN_HAND_NAMES:
        count = hand_counts[key]
        if count > 0:
            hand_sfen_chars.append(f'{count}{key}' if count > 1 else f'{key}')

    hand_sfen = ''.join(hand_sfen_chars) if hand_sfen_chars else '-'

    # Return SFEN format
    return f'{board_sfen} {color_sfen} {hand_sfen} 1'
