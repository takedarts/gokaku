import unittest

from deepshogi.psfen import convert_psfen_to_sfen, convert_sfen_to_psfen


class PsfenTest(unittest.TestCase):
    '''Check hand counts without external record fixtures.'''
    def test_convert_sfen_to_psfen_hand_counts(self) -> None:
        '''Check hand counts and resetting counts between piece types.
        '''
        # Cover omitted counts, single and double digits, and switching piece types.
        hands = ['-', 'P', 'p', '9P', '9p', '17Pp', 'P17p', '10P8p', '8P10p',
                 '18Pl', 'L18p', '2R18P']
        hands.extend(f'{count}{piece}' for piece in ('P', 'p') for count in range(10, 19))

        # Vary turn and hand ownership independently and compare round trips with expected SFEN.
        for color in ('b', 'w'):
            for hand in hands:
                with self.subTest(color=color, hand=hand):
                    sfen = f'4k4/9/9/9/9/9/9/9/4K4 {color} {hand} 1'
                    psfen = convert_sfen_to_psfen(sfen)

                    self.assertEqual(len(psfen), 32)
                    self.assertEqual(convert_psfen_to_sfen(psfen), sfen)


    def test_convert_sfen_to_psfen_distinguishes_hand_counts(self) -> None:
        '''Encode double-digit hand counts differently from their units digits.
        '''
        # Compare count pairs that previously produced the same Packed SFEN.
        for piece in ('P', 'p'):
            for count in range(10, 19):
                with self.subTest(piece=piece, count=count):
                    lower_count = count - 10
                    lower_hand = f'{lower_count}{piece}' if lower_count > 1 else piece
                    if lower_count == 0:
                        lower_hand = '-'
                    sfen = f'4k4/9/9/9/9/9/9/9/4K4 b {count}{piece} 1'
                    lower_sfen = f'4k4/9/9/9/9/9/9/9/4K4 b {lower_hand} 1'

                    self.assertNotEqual(
                        convert_sfen_to_psfen(sfen), convert_sfen_to_psfen(lower_sfen))

