import logging
import math
import random
import time
from typing import Dict, List, Tuple

from .board import Board, is_hand_position
from .config import (BOARD_SIZE, COLOR_NONE, COLOR_WHITE,
                     DEFAULT_ALLOWED_REPEATS, DEFAULT_CHECK_NODE_DEPTH,
                     DEFAULT_CHECK_SEARCH_DEPTH, DEFAULT_CHECK_SEARCH_NODE,
                     DEFAULT_DRAW_TURN, DEFAULT_INITIAL_SFEN,
                     DEFAULT_MAX_VISITS, DEFAULT_NYUGYOKU_SCORES,
                     DEFAULT_PUCB_CONSTANT_BASE, DEFAULT_PUCB_CONSTANT_INIT,
                     DEFAULT_PUCB_MIN_VISITS_RATE, RESULT_MAX_MOVES,
                     RESULT_NONE, RESULT_NYUGYOKU, RESULT_SENNICHITE,
                     RESULT_TSUMI, get_color_name, get_opposite_color)
from .exception import ShogiException
from .native import NativePlayer
from .processor import Processor
from .psfen import convert_sfen_to_psfen

LOGGER = logging.getLogger(__name__)


def parse_move16(move: int) -> Tuple[Tuple[int, int], Tuple[int, int], bool]:
    '''Parse move representation.
    Args:
        move (int): move representation
    Returns:
        Tuple[Tuple[int,int], Tuple[int,int], bool]: Source, destination, True if promote
    '''
    src = (move >> 7) & 0x7f
    dst = (move >> 0) & 0x7f
    promote = (move & 0x4000) != 0

    return (
        (src // BOARD_SIZE, src % BOARD_SIZE),
        (dst // BOARD_SIZE, dst % BOARD_SIZE),
        promote)


class Candidate(object):
    '''Hold a candidate move, its evaluation, and its principal variation.'''
    def __init__(
        self,
        src: Tuple[int, int],
        dst: Tuple[int, int],
        promote: bool,
        color: int,
        visits: int,
        policy: float,
        value: float,
        remaining_turns: float,
        variations: List[int],
    ) -> None:
        '''Initialize a candidate move.
        Args:
            src (Tuple[int, int]): Source coordinates.
            dst (Tuple[int, int]): Destination coordinates.
            promote (bool): True if the move promotes the piece.
            color (int): Player color.
            visits (int): Visit count.
            policy (float): Predicted move probability.
            value (float): Predicted outcome value.
            remaining_turns (float): Predicted moves remaining until the game ends.
            variations (List[int]): Principal variation as 16-bit move encodings.
        '''
        self.src = src
        self.dst = dst
        self.promote = promote
        self.color = color
        self.visits = visits
        self.policy = policy
        self.value = value
        self.remaining_turns = max(remaining_turns, 0.0)
        self.variations = [parse_move16(v) for v in variations]

        if math.isnan(self.policy):
            raise ShogiException('policy is NaN')

        if math.isnan(self.value):
            raise ShogiException('value is NaN')

        self.value_lcb = value - color * 1.96 * 0.5 / (visits + 1)**0.5

    @property
    def win_chance(self) -> float:
        '''Returns the win rate.
        Returns:
            float: Win rate
        '''
        return self.value * self.color * 0.5 + 0.5

    @property
    def win_chance_lcb(self) -> float:
        '''Return the lower confidence bound of the win probability.
        Returns:
            float: Lower confidence bound of the win probability.
        '''
        return self.value_lcb * self.color * 0.5 + 0.5

    def __str__(self) -> str:
        '''Return a readable description of the candidate as str.'''
        return (
            f'Candidate('
            f'src={self.src}, dst={self.dst}, promote={self.promote},'
            f' color={get_color_name(self.color)},'
            f' visits={self.visits}, policy={self.policy:.2f},'
            f' value={self.value:.3f}, value_lcb={self.value_lcb:.3f},'
            f' remaining_turns={self.remaining_turns:.1f},'
            f' win_chance={self.win_chance:.3f}, win_chance_lcb={self.win_chance_lcb:.3f},'
            f' variations={self.variations})')

    def __repr__(self) -> str:
        '''Return the candidate representation as str.'''
        return str(self)


class Referee(object):
    '''Determine game results.'''

    def __init__(
        self,
        allowed_repeats: int = DEFAULT_ALLOWED_REPEATS,
        draw_turn: int = DEFAULT_DRAW_TURN,
    ) -> None:
        '''Initialize referee object.
        Args:
            allowed_repeats (int): Allowed number of repeats of the same position (default is 3)
            draw_turn (int): Number of moves for a draw (default is 512)
        '''
        self.allowed_repeats = allowed_repeats
        self.draw_turn = draw_turn

        self.board_repeats: Dict[bytes, Tuple[int, int]] = {}
        self.check_counts = [0, 0]

    def clear(self) -> None:
        '''Clear the state.'''
        self.board_repeats.clear()
        self.check_counts = [0, 0]

    def update(self, board: Board) -> None:
        '''Update the state.'''
        # Register the current board position in the history
        repeat_key = convert_sfen_to_psfen(board.get_sfen())
        repeat_values = self.board_repeats.get(repeat_key, (0, board.get_turn()))
        self.board_repeats[repeat_key] = (repeat_values[0] + 1, repeat_values[1])

        # If in check, count consecutive checks
        color_index = 0 if board.get_color() == COLOR_WHITE else 1

        if board.is_check():
            self.check_counts[color_index] += 1
        else:
            self.check_counts[color_index] = 0

    def judge(self, board: Board) -> Tuple[bool, int, int]:
        '''Judge the game result.
        Args:
            board (Board): Board data
        Returns:
            Tuple[bool, int, int]: True if the game is over, winner's side, end reason
        '''
        # If there are no legal moves, judge as checkmate
        if len(board.get_legal_moves()) == 0:
            return True, get_opposite_color(board.get_color()), RESULT_TSUMI

        # If nyugyoku declaration is possible, judge as win by nyugyoku declaration
        if board.is_nyugyoku():
            return True, board.get_color(), RESULT_NYUGYOKU

        # If the number of moves exceeds the draw threshold, judge as a draw
        if board.get_turn() >= self.draw_turn:
            return True, COLOR_NONE, RESULT_MAX_MOVES

        # Judge whether it is repetition (sennichite)
        repeat_key = convert_sfen_to_psfen(board.get_sfen())
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

        # Otherwise, judge that the game is ongoing
        return False, COLOR_NONE, RESULT_NONE

    def get_repeats(self, board: Board) -> int:
        '''Return the number of repeats of the same position.
        Args:
            board (Board): Board data
        Returns:
            int: Number of repeats of the same position
        '''
        repeat_key = convert_sfen_to_psfen(board.get_sfen())
        repeat_values = self.board_repeats.get(repeat_key, (0, 0))

        return repeat_values[0]


class Player(object):
    '''Coordinate native search with game adjudication.'''
    def __init__(
        self,
        processor: Processor,
        threads: int = 1,
        max_visits: int = DEFAULT_MAX_VISITS,
        initial_sfen: str = DEFAULT_INITIAL_SFEN,
        nyugyoku_scores: Tuple[int, int] = DEFAULT_NYUGYOKU_SCORES,
        draw_turn: int = DEFAULT_DRAW_TURN,
        sennichite_penalty: float = 0.0,
        check_search_depth: int = DEFAULT_CHECK_SEARCH_DEPTH,
        check_search_node: int = DEFAULT_CHECK_SEARCH_NODE,
        check_node_depth: int = DEFAULT_CHECK_NODE_DEPTH,
        pucb_constant_init: float = DEFAULT_PUCB_CONSTANT_INIT,
        pucb_constant_base: float = DEFAULT_PUCB_CONSTANT_BASE,
        pucb_min_visits_rate: float = DEFAULT_PUCB_MIN_VISITS_RATE,
        allowed_repeats: int = DEFAULT_ALLOWED_REPEATS,
        check_next_repeats: bool = True,
    ) -> None:
        '''Initialize the player.
        Args:
            processor (Processor): Inference processor.
            threads (int): Number of threads to use.
            max_visits (int): Maximum node visit count.
            initial_sfen (str): Initial position in SFEN format.
            nyugyoku_scores (Tuple[int, int]): Points required for entering-king declarations.
            draw_turn (int): Move count at which the game is drawn.
            sennichite_penalty (float): Penalty assigned to repetition evaluations.
            check_search_depth (int): Checkmate search depth.
            check_search_node (int): Checkmate search node capacity.
            check_node_depth (int): Node depth at which to run checkmate search.
            pucb_constant_init (float): Initial PUCB exploration coefficient.
            pucb_constant_base (float): Base controlling the PUCB exploration coefficient.
            pucb_min_visits_rate (float): Minimum visit ratio for prioritizing PUCB children.
            allowed_repeats (int): Allowed position repetitions; defaults to three.
            check_next_repeats (bool): Whether to check repetition on the following turn.
        '''
        # Keep a reference to the processor object so it is not destroyed
        self.processor = processor

        # Create the native object.
        self.native = NativePlayer(
            processor.native, threads, max_visits, nyugyoku_scores, draw_turn,
            sennichite_penalty, check_search_depth, check_search_node, check_node_depth,
            pucb_constant_init, pucb_constant_base, pucb_min_visits_rate)

        self.native.initialize(initial_sfen)
        self.sennichite_penalty = sennichite_penalty

        # Create the game referee.
        self.referee = Referee(
            allowed_repeats=allowed_repeats, draw_turn=draw_turn)
        self.check_next_repeats = check_next_repeats

    def initialize(self, sfen: str = DEFAULT_INITIAL_SFEN) -> None:
        '''Initialize state.
        Args:
            sfen (str): Initial board in SFEN format
        '''
        # Stop pondering if it is active.
        self.native.wait_evaluation(0, 0.0, True)

        # Initialize the position.
        self.native.initialize(sfen)
        self.referee.clear()

    def play(
        self,
        src: Tuple[int, int],
        dst: Tuple[int, int],
        promote: bool = False,
        piece: int | None = None,
    ) -> Tuple[Tuple[int, int], Tuple[int, int], bool]:
        '''Move a piece.
        Return the move information (source, destination, True if promote).
        Args:
            src (Tuple[int, int]): Source coordinates
            dst (Tuple[int, int]): Destination coordinates
            promote (bool): True if promote
            piece (int): Type of piece after moving
        Returns:
            Tuple[Tuple[int, int], Tuple[int, int], bool]: Move information
        '''
        # determine whether to promote based on the piece type
        if piece is not None and not is_hand_position(src):
            promote = (self.get_board().get_piece(src) != piece)

        # Update the repetition judgment object
        self.referee.update(self.get_board())

        # Play the move.
        self.native.play(src, dst, promote)

        return src, dst, promote

    def get_random_candidate(
        self,
        width: int = 16,
        timelimit: float = 120.0,
        temperature: float = 1.0,
        noise: float = 0.0,
        delta: float = 0.1,
        ponder: bool = False,
    ) -> Candidate:
        '''Return a candidate sampled from the policy distribution.
        Args:
            width (int): Number of candidate moves.
            timelimit (float): Time limit in seconds.
            temperature (float): Temperature parameter.
            noise (float): Strength of Gumbel noise during search.
            delta (float): Acceptable drop in win probability.
            ponder (bool): True to continue searching.
        Returns:
            Candidate: Selected candidate move.
        '''
        # Evaluate the position.
        self.native.start_evaluation(True, width, 1.0, noise)
        self.native.wait_evaluation(width + 1, timelimit, not ponder)

        # Build the candidate list.
        candidates = [Candidate(*c) for c in self.native.get_candidates()]

        # Get the maximum predicted win rate
        max_win_chance = max(c.win_chance for c in candidates)

        # Exclude candidate moves whose predicted win rate is more than delta below the maximum
        candidates = [
            c for c in candidates if c.win_chance >= max_win_chance - delta]

        # Convert policy values to selection probabilities
        probs = [c.policy**(1 / max(temperature, 1e-3)) for c in candidates]

        # Randomly select a candidate.
        candidate = random.choices(candidates, weights=probs, k=1)[0]

        # Output log
        if LOGGER.isEnabledFor(logging.DEBUG):
            LOGGER.debug(
                'Random: %d candidates, max_win_chance=%.3f, delta=%.3f, temperature=%.3f',
                len(candidates), max_win_chance, delta, temperature)
            LOGGER.debug(candidate)

        return candidate

    def evaluate(
        self,
        visits: int,
        timelimit: float = 120.0,
        equally: bool = False,
        criterion: str = 'value',
        candidate_width: int = 0,
        temperature: float = 1.0,
        noise: float = 0.0,
        ponder: bool = False,
        extends: int = 0,
    ) -> List[Candidate]:
        '''Evaluate the position.
        Args:
            visits (int): Target visit count.
            timelimit (float): Total time limit in seconds, including search extensions.
            equally (bool): True for equal visits, False for PUCB or similar selection.
            criterion (str): Candidate ranking criterion: 'value' or 'visits'.
            candidate_width (int): Search width; zero adjusts the width automatically.
            temperature (float): Search temperature.
            noise (float): Strength of Gumbel noise during search.
            ponder (bool): True to continue searching.
            extends (int): Maximum number of search extensions.
        Returns:
            List[Candidate]: Candidate moves.
        '''
        # Set the shared deadline and visit increment for search extensions.
        deadline = time.monotonic() + timelimit
        additional_visits = max(visits // 2, 1)
        candidates: List[Candidate] = []
        repeats = 0

        # Clamp the extension limit to zero or greater.
        extends = max(extends, 0)

        # Run the initial search and at most the requested number of extensions.
        while repeats <= extends:
            # Skip extensions when less than one second remains.
            remaining_time = max(deadline - time.monotonic(), 0.0)

            if repeats > 0 and remaining_time < 1.0:
                break

            # Increase the cumulative visit target relative to the original request.
            target_visits = visits + repeats * additional_visits

            # Evaluate the position.
            LOGGER.debug('Evaluation: %d visits, %.1f seconds', target_visits, remaining_time)
            self.native.start_evaluation(equally, candidate_width, temperature, noise)

            # Include time spent waiting for search startup in the time limit.
            remaining_time = max(deadline - time.monotonic(), 0.0)
            self.native.wait_evaluation(target_visits, remaining_time, not ponder)

            # Build the candidate list.
            candidates = [Candidate(*c) for c in self.native.get_candidates()]

            # Do not extend the search if no candidates are available.
            if not candidates:
                break

            # Reflect adjudicated game results in the evaluations.
            for candidate in candidates:
                # Create a board after making the candidate move
                board = self.get_board()
                board.play(candidate.src, candidate.dst, candidate.promote)

                # Judge the end of the game on the board after making the candidate move
                game_over, winner, result = self.referee.judge(board)

                # Use the winner's color as the evaluation when the game has ended.
                if game_over:
                    if result == RESULT_SENNICHITE and winner == COLOR_NONE:
                        value = get_opposite_color(candidate.color) * self.sennichite_penalty
                    else:
                        value = winner

                    candidate.value = value
                    candidate.value_lcb = value

            # Sort the candidates.
            if criterion == 'visits':
                candidates.sort(key=lambda cand: cand.visits, reverse=True)
            else:
                candidates.sort(key=lambda cand: cand.win_chance_lcb, reverse=True)

            # Output log
            if LOGGER.isEnabledFor(logging.DEBUG):
                LOGGER.debug(
                    'Evaluation: %d visits (batch fill rate=%.2f, cache hit rate=%.2f)',
                    sum(c.visits for c in candidates),
                    self.processor.get_batch_fill_rate(),
                    self.processor.get_cache_hit_rate())
                for candidate in candidates:
                    LOGGER.debug(candidate)

            # Stop if the selected move's win probability is outside the extension range.
            if not (0.05 < candidates[0].win_chance < 0.95):
                break

            # Stop unless another candidate has at least two thirds of the selected move's visits.
            if not any(c.visits * 3 >= candidates[0].visits * 2 for c in candidates[1:]):
                break

            # Advance the extension counter.
            repeats += 1

        # Return the candidate list.
        return candidates

    def stop_evaluation(self) -> None:
        '''Stop pondering if it is active.'''
        self.native.wait_evaluation(0, 0.0, True)

    def get_color(self) -> int:
        '''Return the side to move.
        Returns:
            int: Side to move.
        '''
        return self.native.get_color()

    def get_board(self) -> Board:
        '''Return board data.
        Returns:
            Board: Board data
        '''
        board = Board()

        self.native.copy_board_to(board.native)

        return board

    def get_visits(self) -> int:
        '''Return the number of visits to the root node.
        Returns:
            int: Number of visits to the root node
        '''
        return self.native.get_visits()

    def __str__(self) -> str:
        '''Return the string representation.
        Returns:
            str: String representation
        '''
        return self.native.to_string()
