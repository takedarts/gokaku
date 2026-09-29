'''Exercise threaded search lifecycle with a model supplied by the regression test.'''

import sys

from deepshogi.player import Player
from deepshogi.processor import Processor


def main(path: str) -> None:
    '''Search using path (str), checking stop/play/reset transitions; return None.'''
    processor = Processor(path, [-1], False, True, 4, 1, 8)
    player = Player(
        processor, threads=4, max_visits=128, check_search_depth=3,
        check_search_node=100, check_node_depth=1, pucb_min_visits_rate=0.1,
        sennichite_penalty=0.2)
    # Alternate ongoing searches, explicit stops, root changes, and resets.
    for index in range(8):
        candidates = player.evaluate(32, timelimit=1.0, ponder=True, extends=1)
        assert candidates
        chosen = candidates[0]
        assert (chosen.src, chosen.dst, chosen.promote) in player.get_board().get_legal_moves()
        assert abs(chosen.remaining_turns - 25.0) < 1e-4
        player.stop_evaluation()
        player.play(chosen.src, chosen.dst, chosen.promote)
        if index % 2:
            player.initialize()
    player.stop_evaluation()


if __name__ == '__main__':
    main(sys.argv[1])
