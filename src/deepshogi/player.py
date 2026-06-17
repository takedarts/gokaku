import logging
import math
import random
from typing import Dict, List, Tuple

from .board import Board, is_hand_position
from .config import (BOARD_SIZE, COLOR_BLACK, COLOR_NONE, COLOR_WHITE,
                     DEFAULT_ALLOWED_REPEATS, DEFAULT_CHECK_NODE_DEPTH,
                     DEFAULT_CHECK_SEARCH_DEPTH, DEFAULT_CHECK_SEARCH_NODE,
                     DEFAULT_DRAW_TURN, DEFAULT_INITIAL_SFEN,
                     DEFAULT_MAX_VISITS, DEFAULT_NYUGYOKU_SCORES,
                     DEFAULT_PUCB_CONSTANT_BASE, DEFAULT_PUCB_CONSTANT_INIT,
                     RESULT_MAX_MOVES, RESULT_NONE, RESULT_NYUGYOKU,
                     RESULT_SENNICHITE, RESULT_TSUMI, get_color_name,
                     get_opposite_color)
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
        '''Initialize candidate move object.
        Args:
            src (Tuple[int, int]): Source coordinates
            dst (Tuple[int, int]): Destination coordinates
            promote (bool): True if promote
            color (int): Side to move
            visits (int): Number of visits
            playouts (int): Number of playouts
            policy (float): Expected move probability
            value (float): Predicted win rate
            variations (List[int]): Expected sequence (move representation)
        '''
        self.src = src
        self.dst = dst
        self.promote = promote
        self.color = color
        self.visits = visits
        self.playouts = playouts
        self.policy = policy
        self.value = value
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
        '''Returns the lower confidence bound of the win rate.
        Returns:
            float: Lower confidence bound of the win rate
        '''
        return self.value_lcb * self.color * 0.5 + 0.5

    def __str__(self) -> str:
        return (
            f'Candidate('
            f'src={self.src}, dst={self.dst}, promote={self.promote},'
            f' color={get_color_name(self.color)},'
            f' visits={self.visits}, playouts={self.playouts}, policy={self.policy:.2f},'
            f' value={self.value:.3f}, value_lcb={self.value_lcb:.3f},'
            f' win_chance={self.win_chance:.3f}, win_chance_lcb={self.win_chance_lcb:.3f},'
            f' variations={self.variations})')

    def __repr__(self) -> str:
        return str(self)


class Referee(object):
    '''Class to judge game results.'''

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
    def __init__(
        self,
        processor: Processor,
        threads: int = 1,
        max_visits: int = DEFAULT_MAX_VISITS,
        initial_sfen: str = DEFAULT_INITIAL_SFEN,
        nyugyoku_scores: Tuple[int, int] = DEFAULT_NYUGYOKU_SCORES,
        draw_turn: int = DEFAULT_DRAW_TURN,
        check_search_depth: int = DEFAULT_CHECK_SEARCH_DEPTH,
        check_search_node: int = DEFAULT_CHECK_SEARCH_NODE,
        check_node_depth: int = DEFAULT_CHECK_NODE_DEPTH,
        pucb_constant_init: float = DEFAULT_PUCB_CONSTANT_INIT,
        pucb_constant_base: float = DEFAULT_PUCB_CONSTANT_BASE,
        allowed_repeats: int = DEFAULT_ALLOWED_REPEATS,
        check_next_repeats: bool = True,
    ) -> None:
        '''Initialize player object.
        Args:
            processor (Processor): Processor management object
            threads (int): Number of threads to use
            max_visits (int): Maximum number of visits for search
            initial_sfen (str): Initial board in SFEN format
            nyugyoku_scores (Tuple[int, int]): Points required for nyugyoku declaration
            draw_turn (int): Number of turns for a draw
            check_search_depth (int): Depth for checkmate search
            check_search_node (int): Number of nodes for checkmate search
            check_node_depth (int): Maximum depth of nodes for checkmate search
            pucb_constant_init (float): Initial value applied to PUCB upper confidence bound
            pucb_constant_base (float): Base value applied to PUCB upper confidence bound
            allowed_repeats (int): Allowed number of repeats of the same position (default is 3)
            check_next_repeats (bool): True to judge repetition for the next side to move
        '''
        # Keep a reference to the processor object so it is not destroyed
        self.processor = processor

        # Create native object
        self.native = NativePlayer(
            processor.native, threads, max_visits, nyugyoku_scores, draw_turn,
            check_search_depth, check_search_node, check_node_depth,
            pucb_constant_init, pucb_constant_base)

        self.native.initialize(initial_sfen)

        # Create referee object for the game
        self.referee = Referee(
            allowed_repeats=allowed_repeats, draw_turn=draw_turn)
        self.check_next_repeats = check_next_repeats

    def initialize(self, sfen: str = DEFAULT_INITIAL_SFEN) -> None:
        '''Initialize state.
        Args:
            sfen (str): Initial board in SFEN format
        '''
        # Stop if pondering
        self.native.wait_evaluation(0, 0, 0.0, True)

        # Initialize
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
        # If the type of piece after moving is specified,
        # determine whether to promote based on the piece type
        if piece is not None and not is_hand_position(src):
            promote = (self.get_board().get_piece(src) != piece)

        # Update the repetition judgment object
        self.referee.update(self.get_board())

        # Move the piece
        self.native.play(src, dst, promote)

        return src, dst, promote

    def get_random(
        self,
        width: int = 16,
        timelimit: float = 120.0,
        temperature: float = 1.0,
        delta: float = 0.1,
        ponder: bool = False,
    ) -> Candidate:
        '''Return a random move.
        Args:
            width (int): Number of candidate moves
            timelimit (float): Time limit (seconds)
            temperature (float): Temperature parameter
            delta (float): Allowable win rate drop
            ponder (bool): True to continue searching
        Returns:
            Candidate: Candidate move
        '''
        # Evaluate the board
        self.native.start_evaluation(True, width, 1.0, 0.0)
        self.native.wait_evaluation(width + 1, 0, timelimit, not ponder)

        # Create a list of candidate moves
        candidates = [Candidate(*c) for c in self.native.get_candidates()]

        # Get the maximum predicted win rate
        max_win_chance = max(c.win_chance for c in candidates)

        # Exclude candidate moves whose predicted win rate is more than delta below the maximum
        candidates = [
            c for c in candidates if c.win_chance >= max_win_chance - delta]

        # Convert policy values to selection probabilities
        probs = [c.policy**(1 / max(temperature, 1e-3)) for c in candidates]

        # Return a randomly selected candidate move
        return random.choices(candidates, weights=probs, k=1)[0]

    def evaluate(
        self,
        visits: int,
        playouts: int = 0,
        timelimit: float = 120.0,
        equally: bool = False,
        criterion: str = 'value',
        candidate_width: int = 0,
        temperature: float = 1.0,
        noise: float = 0.0,
        sennichite_penalty: float = 0.0,
        ponder: bool = False,
    ) -> List[Candidate]:
        '''Evaluate the board.
        Args:
            visits (int): Target number of visits
            playouts (int): Target number of playouts
            timelimit (float): Time limit (seconds)
            equally (bool): True to make the number of searches equal, False to use UCB or PUCB
            criterion (str): Criterion for prioritizing candidate moves ('value' or 'visits')
            candidate_width (int): Search width for candidate moves (if 0, width is automatically adjusted)
            temperature (float): Temperature parameter for search
            noise (float): Strength of Gumbel noise for search
            sennichite_penalty (float): Penalty to set for repetition (sennichite) evaluation
            ponder (bool): True to continue searching
        Returns:
            List[Candidate]: List of candidate moves
        '''
        # Evaluate the board
        LOGGER.debug(
            'Evaluation: %d visits, %d playouts, %.1f seconds',
            visits, playouts, timelimit)
        self.native.start_evaluation(equally, candidate_width, temperature, noise)
        self.native.wait_evaluation(visits, playouts, timelimit, not ponder)

        # Create a list of candidate moves
        candidates = [Candidate(*c) for c in self.native.get_candidates()]

        # Judge the game result and reflect it in the evaluation value
        for candidate in candidates:
            # Create a board after making the candidate move
            board = self.get_board()
            board.play(candidate.src, candidate.dst, candidate.promote)

            # Judge the end of the game on the board after making the candidate move
            game_over, winner, result = self.referee.judge(board)

            # If the game is over, set the winner's side as the evaluation value
            # If it is a draw (sennichite), set the specified penalty
            if game_over:
                if result == RESULT_SENNICHITE and winner == COLOR_NONE:
                    value = get_opposite_color(candidate.color) * sennichite_penalty
                else:
                    value = winner

                candidate.value = value
                candidate.value_lcb = value
                continue

            # If not judging repetition for the next side to move, proceed to the next
            # candidate move
            if not self.check_next_repeats:
                continue

            # If it is a draw (sennichite) for the next side to move, also set the specified penalty
            next_board = Board()

            for move in board.get_legal_moves():
                next_board.copy_from(board)
                next_board.play(*move)

                repeats = self.referee.get_repeats(next_board)

                if repeats < self.referee.allowed_repeats:
                    continue
                elif candidate.color == COLOR_BLACK:
                    candidate.value = min(candidate.value, -1 * sennichite_penalty)
                    candidate.value_lcb = min(candidate.value_lcb, -1 * sennichite_penalty)
                    break
                else:
                    candidate.value = max(candidate.value, sennichite_penalty)
                    candidate.value_lcb = max(candidate.value_lcb, sennichite_penalty)
                    break

        # Sort candidate moves
        if criterion == 'visits':
            candidates.sort(key=lambda cand: cand.visits, reverse=True)
        else:
            candidates.sort(key=lambda cand: cand.win_chance_lcb, reverse=True)

        # Output logs
        if LOGGER.isEnabledFor(logging.DEBUG):
            LOGGER.debug(
                'Evaluation: %d visits, %d playouts (batch fill rate=%.2f, cache hit rate=%.2f)',
                sum(c.visits for c in candidates),
                sum(c.playouts for c in candidates),
                self.processor.get_batch_fill_rate(),
                self.processor.get_cache_hit_rate())
            for candidate in candidates:
                LOGGER.debug(candidate)

        # Return the list of candidate moves
        return candidates

    def stop_evaluation(self) -> None:
        '''Stop if pondering.'''
        self.native.wait_evaluation(0, 0, 0.0, True)

    def get_color(self) -> int:
        '''Return the side to move.
        Returns:
            int: Side to move
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
