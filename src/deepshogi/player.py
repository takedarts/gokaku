import logging
import math
import random
import time
from threading import Event
from typing import Dict, List, Tuple

from .board import Board, is_hand_position
from .config import (BOARD_SIZE, COLOR_NONE,
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
from .validation import (INT32_MAX, validate_nonnegative_float, validate_positive_float,
                         validate_positive_int)

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
        '''Return a readable description of the candidate as str.
        Returns:
            str: Formatted candidate evaluation and principal variation.
        '''
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
        '''Return the candidate representation as str.
        Returns:
            str: Formatted candidate evaluation and principal variation.
        '''
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
            draw_turn (int): Number of moves for a draw (default is 512).
        '''
        self.allowed_repeats = allowed_repeats
        self.draw_turn = draw_turn

        self.board_repeats: Dict[bytes, Tuple[int, int]] = {}
        self.check_history: Dict[int, Tuple[int, bool]] = {}

    def clear(self) -> None:
        '''Clear the state.'''
        self.board_repeats.clear()
        self.check_history.clear()

    def update(self, board: Board) -> None:
        '''Register the position and whether the move leading to it was a check.
        Args:
            board (Board): Position to register in the history.
        '''
        # Register the current board position in the history
        repeat_key = convert_sfen_to_psfen(board.get_sfen())
        repeat_values = self.board_repeats.get(repeat_key, (0, board.get_turn()))
        self.board_repeats[repeat_key] = (repeat_values[0] + 1, repeat_values[1])

        # Store whether the move by the opponent of the side to move was a check
        self.check_history[board.get_turn()] = (
            get_opposite_color(board.get_color()), board.is_check())

    def judge(
        self, board: Board, previous_board: Board | None = None,
    ) -> Tuple[bool, int, int]:
        '''Judge the game result.
        Args:
            board (Board): Board data
            previous_board (Board | None): Unregistered position immediately before a trial move
        Returns:
            Tuple[bool, int, int]: True if the game is over, winner's side, end reason
        '''
        # If there are no legal moves, judge as checkmate
        if len(board.get_legal_moves()) == 0:
            return True, get_opposite_color(board.get_color()), RESULT_TSUMI

        # If nyugyoku declaration is possible, judge as win by nyugyoku declaration
        if board.is_nyugyoku():
            return True, board.get_color(), RESULT_NYUGYOKU

        # Judge whether it is repetition (sennichite)
        repeat_key = convert_sfen_to_psfen(board.get_sfen())
        current_turn = board.get_turn()
        repeat_values = self.board_repeats.get(repeat_key, (0, current_turn))
        repeat_count, repeat_turn = repeat_values

        if repeat_count >= self.allowed_repeats:
            # Add the judged position and the unregistered position before the trial move
            # only for this judgment
            pending = {current_turn: (get_opposite_color(board.get_color()), board.is_check())}
            if previous_board is not None:
                pending[previous_board.get_turn()] = (
                    get_opposite_color(previous_board.get_color()), previous_board.is_check())

            # Check all moves of each side, excluding the move to the first identical position
            checking_colors: set[int] = set()
            other_colors: set[int] = set()
            for turn in range(repeat_turn + 1, current_turn + 1):
                entry = pending.get(turn, self.check_history.get(turn))
                if entry is None:
                    return True, COLOR_NONE, RESULT_SENNICHITE
                color, check = entry
                (checking_colors if check else other_colors).add(color)

            # If exactly one side kept checking, the opponent of that side wins
            continuous = checking_colors - other_colors
            winner = get_opposite_color(continuous.pop()) if len(continuous) == 1 else COLOR_NONE
            return True, winner, RESULT_SENNICHITE

        # If no repetition occurs and the maximum move count is reached, judge as a draw
        if current_turn >= self.draw_turn:
            return True, COLOR_NONE, RESULT_MAX_MOVES

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
        sennichite_penalty: float = 0.0,
        check_search_depth: int = DEFAULT_CHECK_SEARCH_DEPTH,
        check_search_node: int = DEFAULT_CHECK_SEARCH_NODE,
        check_node_depth: int = DEFAULT_CHECK_NODE_DEPTH,
        pucb_constant_init: float = DEFAULT_PUCB_CONSTANT_INIT,
        pucb_constant_base: float = DEFAULT_PUCB_CONSTANT_BASE,
        pucb_min_visits_rate: float = DEFAULT_PUCB_MIN_VISITS_RATE,
        allowed_repeats: int = DEFAULT_ALLOWED_REPEATS,
    ) -> None:
        '''Initialize the player.
        Args:
            processor (Processor): Inference processor.
            threads (int): Number of search threads.
            max_visits (int): Maximum node visit count.
            initial_sfen (str): Initial board in SFEN format.
            nyugyoku_scores (Tuple[int, int]): Points required for entering-king declarations.
            sennichite_penalty (float): Finite repetition penalty; negative values are clamped to
                zero.
            check_search_depth (int): Checkmate search depth.
            check_search_node (int): Checkmate node capacity.
            check_node_depth (int): Node depth at which to search for checkmate.
            pucb_constant_init (float): Initial PUCB coefficient.
            pucb_constant_base (float): PUCB base; must remain finite and positive as a 32-bit
                float.
            pucb_min_visits_rate (float): Minimum visit ratio for prioritizing PUCB children.
            allowed_repeats (int): Allowed repetitions of the same position (default: three).
        '''
        # Reject unusable search configurations before constructing the native player.
        validate_positive_int('threads', threads)
        validate_positive_int('max_visits', max_visits)
        validate_positive_float('pucb_constant_base', pucb_constant_base)

        # Keep a reference to the processor object so it is not destroyed
        self.processor = processor

        # Create the native object.
        self.native = NativePlayer(
            processor.native, threads, max_visits, nyugyoku_scores,
            sennichite_penalty, check_search_depth, check_search_node, check_node_depth,
            pucb_constant_init, pucb_constant_base, pucb_min_visits_rate)

        # Use the same clamped penalty as native code when evaluating terminal positions.
        self.sennichite_penalty = max(sennichite_penalty, 0.0)

        # Create the game referee.
        self.referee = Referee(allowed_repeats=allowed_repeats)

        # Set the initial position and default draw limit.
        self.initialize(initial_sfen)

    def initialize(
        self,
        sfen: str = DEFAULT_INITIAL_SFEN,
        draw_turn: int = DEFAULT_DRAW_TURN,
        *,
        nyugyoku_scores: Tuple[int, int] | None = None,
    ) -> None:
        '''Initialize state.
        Args:
            sfen (str): Initial board in SFEN format
            draw_turn (int): Total move count at which the game is drawn.
            nyugyoku_scores (Tuple[int, int] | None): Points Black and White need for nyugyoku.
                If omitted, the scores given when the player was created are used.
        '''
        # Stop pondering if it is active.
        self.native.wait_evaluation(0, 0.0, True)

        # Update the referee state after the position and game conditions are initialized
        self.native.initialize(sfen, draw_turn, nyugyoku_scores)
        self.referee.clear()
        self.referee.draw_turn = draw_turn

    def play(
        self,
        src: Tuple[int, int],
        dst: Tuple[int, int],
        promote: bool = False,
        piece: int | None = None,
    ) -> Tuple[Tuple[int, int], Tuple[int, int], bool]:
        '''Play a move.
        Return the source coordinates, destination coordinates, and promotion flag.
        Args:
            src (Tuple[int, int]): Source coordinates.
            dst (Tuple[int, int]): Destination coordinates.
            promote (bool): True to promote.
            piece (int | None): Piece type after the move.
        Returns:
            Tuple[Tuple[int, int], Tuple[int, int], bool]: Source, destination, and promotion flag.
        '''
        # Save the previous board to determine promotion and record successful moves.
        board = self.get_board()

        # Infer promotion from the requested piece type after the move.
        if piece is not None and not is_hand_position(src):
            promote = (board.get_piece(src) != piece)

        # Play the move.
        self.native.play(src, dst, promote)

        # Record the previous board for repetition checks only after a successful move.
        self.referee.update(board)

        return src, dst, promote

    def get_random_candidate(
        self,
        width: int = 16,
        timelimit: float = 120.0,
        temperature: float = 1.0,
        noise: float = 0.0,
        delta: float = 0.1,
        ponder: bool = False,
        stop_event: Event | None = None,
    ) -> Candidate | None:
        '''Return a candidate sampled from the policy distribution.
        Args:
            width (int): Number of candidate moves.
            timelimit (float): Time limit in seconds.
            temperature (float): Temperature parameter.
            noise (float): Strength of Gumbel noise during search.
            delta (float): Acceptable drop in win probability (finite and nonnegative).
            ponder (bool): True to continue searching.
            stop_event (Event | None): Optional event requesting cancellation.
        Returns:
            Candidate | None: Selected candidate move, or None if no legal move avoids repetition.
        Raises:
            ValueError: If delta is not finite or is negative.
        '''
        # Validate the tolerance used to compare win probabilities before the native search
        validate_nonnegative_float('delta', delta)

        # Evaluate the position.
        if stop_event is None or not stop_event.is_set():
            self.native.start_evaluation(True, width, 1.0, noise)

            # Notify stopping after startup if a stop request raced with startup.
            if stop_event is not None and stop_event.is_set():
                self.stop_evaluation()

            self.native.wait_evaluation(width + 1, timelimit, not ponder)

        # Exclude candidates that repeat game history before comparing win probabilities
        candidates = self._get_random_candidates()
        if not candidates:
            return None

        # Get the maximum predicted win rate
        max_win_chance = max(c.win_chance for c in candidates)

        # Keep only candidates within the tolerance of the maximum win probability
        candidates = [
            c for c in candidates if c.win_chance >= max_win_chance - delta]

        # Scale the largest weight to one to prevent all weights vanishing at low temperatures.
        max_policy = max(c.policy for c in candidates)
        if max_policy > 0.0:
            temperature_power = 1.0 / max(temperature, 1e-3)
            probs = [(c.policy / max_policy)**temperature_power for c in candidates]
        else:
            # Choose uniformly among remaining candidates when policies contain no preference.
            probs = [1.0] * len(candidates)

        # Randomly select a candidate.
        candidate = random.choices(candidates, weights=probs, k=1)[0]

        # Output log
        if LOGGER.isEnabledFor(logging.DEBUG):
            LOGGER.debug(
                'Random: %d candidates, max_win_chance=%.3f, delta=%.3f, temperature=%.3f',
                len(candidates), max_win_chance, delta, temperature)
            LOGGER.debug(candidate)

        return candidate

    def _get_random_candidates(self) -> List[Candidate]:
        '''Get candidates for a random move that do not return to an earlier position.
        Returns:
            List[Candidate]: Search candidates, one fallback move, or an empty list if none.
        '''
        # Without earlier positions, the search candidates can be used as is
        candidates = [Candidate(*c) for c in self.native.get_candidates()]
        if candidates and not self.referee.board_repeats:
            return candidates

        board = self.get_board()

        def is_new_position(move: Tuple[Tuple[int, int], Tuple[int, int], bool]) -> bool:
            '''Play a trial move on a board copy and check whether the result is in the history.
            Args:
                move (Tuple[Tuple[int, int], Tuple[int, int], bool]): Legal move to try.
            Returns:
                bool: True if the position after the move has not appeared.
            '''
            result = board.play(*move)
            try:
                return self.referee.get_repeats(board) == 0
            finally:
                board.undo(*result)

        # Use the remaining search candidates if any
        candidates = [c for c in candidates if is_new_position((c.src, c.dst, c.promote))]
        if candidates:
            return candidates

        # Search for a nonrepeating fallback from the highest-policy moves, including those
        # outside the search width
        value, remaining_turns, policy_moves = self.native.get_policy_moves()
        ordered_moves = sorted(policy_moves, key=lambda item: item[1], reverse=True)
        policy_choices = (parse_move16(code) for code, _ in ordered_moves)
        move = next((choice for choice in policy_choices if is_new_position(choice)), None)

        # If none can be chosen from the policy, check all legal moves including unpromotions
        if move is None:
            legal_moves = board.get_legal_moves(remove_unpromote=False)
            move = next((choice for choice in legal_moves if is_new_position(choice)), None)
        if move is None:
            return []

        return [Candidate(*move, board.get_color(), 0, 1.0, value, remaining_turns - 1, [])]

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
        stop_event: Event | None = None,
    ) -> List[Candidate]:
        '''Evaluate the board.
        Wait without a deadline until the first visit or confirmed checkmate.
        Cap extension visit targets at the signed 32-bit integer limit.
        Args:
            visits (int): Target node visit count.
            timelimit (float): Time limit in seconds, including extensions.
            equally (bool): True for equal visits; False for PUCB selection.
            criterion (str): Candidate ranking criterion: 'value' or 'visits'.
            candidate_width (int): Candidate search width (zero selects it automatically).
            temperature (float): Search temperature.
            noise (float): Gumbel noise strength.
            ponder (bool): True to continue searching.
            extends (int): Maximum number of search extensions.
            stop_event (Event | None): Stop notification for this search.
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
            # Honor stop requests before starting or extending a search.
            if stop_event is not None and stop_event.is_set():
                break

            # Skip extensions when less than one second remains.
            remaining_time = max(deadline - time.monotonic(), 0.0)

            if repeats > 0 and remaining_time < 1.0:
                break

            # Keep extended visit targets within the int32_t range used by native waits.
            target_visits = min(visits + repeats * additional_visits, INT32_MAX)

            # Evaluate the position.
            LOGGER.debug('Evaluation: %d visits, %.1f seconds', target_visits, remaining_time)
            self.native.start_evaluation(equally, candidate_width, temperature, noise)

            # Preserve stop requests even if native startup clears its stopped state.
            if stop_event is not None and stop_event.is_set():
                self.stop_evaluation()

            # Include time spent waiting for search startup in the time limit.
            remaining_time = max(deadline - time.monotonic(), 0.0)
            self.native.wait_evaluation(target_visits, remaining_time, not ponder)

            # Build the candidate list.
            candidates = [Candidate(*c) for c in self.native.get_candidates()]

            # Do not extend the search if no candidates are available.
            if not candidates:
                break

            # Keep the position before the trial moves and reflect adjudicated game results in the
            # evaluations.
            previous_board = self.get_board()
            for candidate in candidates:
                # Skip further candidate adjudication after stopping.
                if stop_event is not None and stop_event.is_set():
                    break

                # Create a board after making the candidate move
                next_board = self.get_board()
                next_board.play(candidate.src, candidate.dst, candidate.promote)

                # Judge the end of the game on the board after making the candidate move
                game_over, winner, result = self.referee.judge(
                    next_board, previous_board=previous_board)

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

            # Return available results without starting another search after stopping.
            if stop_event is not None and stop_event.is_set():
                break

            # Output log
            if LOGGER.isEnabledFor(logging.DEBUG):
                LOGGER.debug(
                    'Evaluation: %d visits (batch fill rate=%.2f, cache hit rate=%.2f)',
                    sum(c.visits for c in candidates),
                    self.processor.get_batch_fill_rate(),
                    self.processor.get_cache_hit_rate())
                for candidate in candidates:
                    LOGGER.debug(candidate)

            # Stop extending once the visit target reaches the integer limit.
            if target_visits == INT32_MAX:
                break

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
        '''Request stopping and wake native search waits; return None.'''
        self.native.stop_evaluation()

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
