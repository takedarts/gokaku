import logging
import sys
import time
from dataclasses import dataclass, field
from threading import Event, RLock, Thread
from typing import Any, Callable, Dict, List, TextIO, Tuple

from .board import Board
from .config import (AUTHOR, BOARD_SIZE, COLOR_BLACK, COLOR_WHITE,
                     DEFAULT_CHECK_NODE_DEPTH, DEFAULT_CHECK_SEARCH_DEPTH,
                     DEFAULT_CHECK_SEARCH_NODE, DEFAULT_DRAW_TURN,
                     DEFAULT_INITIAL_SFEN, DEFAULT_MAX_VISITS,
                     DEFAULT_NYUGYOKU_SCORES, DEFAULT_PUCB_CONSTANT_BASE,
                     DEFAULT_PUCB_CONSTANT_INIT, DEFAULT_PUCB_MIN_VISITS_RATE,
                     NAME, PIECE_HAND_BISHOP, PIECE_HAND_GOLD,
                     PIECE_HAND_KNIGHT, PIECE_HAND_LANCE, PIECE_HAND_PAWN,
                     PIECE_HAND_ROOK, PIECE_HAND_SILVER, VERSION,
                     get_color_name, get_opposite_color, get_shogi_score)
from .exception import ShogiException
from .player import Candidate, Player
from .processor import Processor
from .timelimit import get_timelimit

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
    '''Parse a USI move string and return move information.
    Args:
        sfen (str): USI move string
    Returns:
        Tuple[Tuple[int,int], Tuple[int,int],bool]: Source, destination, promotion flag.
    '''
    b = sfen.encode('utf-8')

    # If the first character is a number, it's a piece move
    if 49 <= b[0] <= 57:
        src = (b[0] - 49, b[1] - 97)
        dst = (b[2] - 49, b[3] - 97)
        promote = (len(b) >= 5 and b[4] == 43)
    # Otherwise, it's a drop from hand
    elif b[0] in USI_HAND_CHAR_TO_PIECE:
        src = (BOARD_SIZE, USI_HAND_CHAR_TO_PIECE[b[0]])
        dst = (b[2] - 49, b[3] - 97)
        promote = False
    else:
        raise ShogiException('invalid move: {}'.format(sfen))

    return (src, dst, promote)


def usi_move_to_string(src: Tuple[int, int], dst: Tuple[int, int], promote: bool) -> str:
    '''Convert move information to a USI move string.
    Args:
        src (Tuple[int,int]): Source coordinates
        dst (Tuple[int,int]): Destination coordinates
        promote (bool): True if promote
    Returns:
        str: USI move string
    '''
    # If it's a piece move
    if src[0] != BOARD_SIZE:
        src_str = chr(src[0] + 49) + chr(src[1] + 97)
        dst_str = chr(dst[0] + 49) + chr(dst[1] + 97)
        promote_str = '+' if promote else ''
    # If it's a drop from hand
    elif src[0] == BOARD_SIZE and src[1] in USI_HAND_PIECE_TO_CHAR:
        src_str = chr(USI_HAND_PIECE_TO_CHAR[src[1]]) + '*'
        dst_str = chr(dst[0] + 49) + chr(dst[1] + 97)
        promote_str = ''
    else:
        raise ShogiException(f'invalid move: {src}, {dst}, {promote}')

    return f'{src_str}{dst_str}{promote_str}'


def usi_candidate_to_string(candidate: Candidate, index: int) -> str:
    '''Convert candidate move information to a USI analysis result string.
    Args:
        candidate (Candidate): Candidate move information
        index (int): Priority
    Returns:
        str: USI analysis result string
    '''
    nodes = candidate.visits
    score = get_shogi_score(candidate.win_chance)
    text = f'info multipv {index} nodes {nodes} score cp {score}'

    if len(candidate.variations) > 0:
        depth = len(candidate.variations) - 1
        pv = ' '.join(usi_move_to_string(s, d, p) for s, d, p in candidate.variations)
        text += f' depth {depth} pv {pv}'

    return text


@dataclass
class _SearchRequest:
    '''Hold the stop event, response state, and candidates for one go request.'''

    mode: str = 'normal'
    stop_event: Event = field(default_factory=Event)
    discard: bool = False
    responded: bool = False
    candidates: List[Candidate] = field(default_factory=list)


class USIEngine(object):
    '''Run searches in response to USI protocol input.'''

    def __init__(
        self,
        processor: Processor,
        threads: int,
        visits: int = 1,
        extends: int = 0,
        timelimit: float = 0,
        criterion: str = 'value',
        ponder: bool = False,
        multipv: int = 1,
        max_visits: int = DEFAULT_MAX_VISITS,
        resign_threshold: float = 0.0,
        resign_turn: int = 0,
        initial_turn: int = 4,
        initial_width: int = 16,
        initial_temperature: float = 1.0,
        initial_value_delta: float = 0.01,
        initial_white_only: bool = False,
        nyugyoku_scores: Tuple[int, int] = DEFAULT_NYUGYOKU_SCORES,
        draw_turn: int = DEFAULT_DRAW_TURN,
        check_search_depth: int = DEFAULT_CHECK_SEARCH_DEPTH,
        check_search_node: int = DEFAULT_CHECK_SEARCH_NODE,
        check_node_depth: int = DEFAULT_CHECK_NODE_DEPTH,
        pucb_constant_init: float = DEFAULT_PUCB_CONSTANT_INIT,
        pucb_constant_base: float = DEFAULT_PUCB_CONSTANT_BASE,
        pucb_min_visits_rate: float = DEFAULT_PUCB_MIN_VISITS_RATE,
        client_name: str = NAME,
        client_version: str = VERSION,
        client_author: str = AUTHOR,
        reader: TextIO = sys.stdin,
        writer: TextIO = sys.stdout,
    ):
        '''Create the USI engine.
        Args:
            processor (Processor): Inference processor.
            threads (int): Number of threads to use.
            visits (int): Search visit count.
            extends (int): Maximum number of search extensions.
            timelimit (float): Thinking time limit in seconds.
            criterion (str): Candidate ranking criterion: 'value' or 'visits'.
            ponder (bool): True to continue analysis during the opponent's turn.
            multipv (int): Number of candidate variations to report.
            max_visits (int): Maximum search visit count.
            resign_threshold (float): Win probability threshold for resignation.
            resign_turn (int): Minimum move count before resignation.
            initial_turn (int): Number of initial turns using random moves.
            initial_width (int): Number of candidates for random moves.
            initial_temperature (float): Temperature for random moves.
            initial_value_delta (float): Acceptable value difference for random moves.
            initial_white_only (bool): True to use random moves only for White.
            nyugyoku_scores (Tuple[int,int]): Points required for entering-king declarations.
            draw_turn (int): Move count at which the game is drawn.
            check_search_depth (int): Checkmate search depth.
            check_search_node (int): Checkmate search node capacity.
            check_node_depth (int): Node depth at which to run checkmate search.
            pucb_constant_init (float): Initial PUCB coefficient.
            pucb_constant_base (float): Base controlling the PUCB coefficient.
            pucb_min_visits_rate (float): Minimum visit ratio for prioritizing PUCB children.
            client_name (str): Displayed client name.
            client_version (str): Displayed client version.
            client_author (str): Displayed author name.
            reader (TextIO): Command input stream.
            writer (TextIO): Result output stream.
        '''
        self.player: Player | None = None
        self.processor = processor
        self.threads = threads
        self.nyugyoku_scores = nyugyoku_scores
        self.draw_turn = draw_turn
        self.check_search_depth = check_search_depth
        self.check_search_node = check_search_node
        self.check_node_depth = check_node_depth
        self.pucb_constant_init = pucb_constant_init
        self.pucb_constant_base = pucb_constant_base
        self.pucb_min_visits_rate = pucb_min_visits_rate
        self.max_visits = max_visits

        self.visits = visits
        self.extends = extends
        self.timelimit = timelimit
        self.criterion = criterion
        self.ponder = ponder
        self.multipv = multipv

        self.resign_threshold = resign_threshold
        self.resign_turn = resign_turn

        self.initial_turn = initial_turn
        self.initial_width = initial_width
        self.initial_temperature = initial_temperature
        self.initial_value_delta = initial_value_delta
        self.initial_white_only = initial_white_only
        self.client_name = client_name
        self.client_version = client_version
        self.client_author = client_author

        self.reader = reader
        self.writer = writer

        self.thread: Thread | None = None
        self.terminated = False
        self._request: _SearchRequest | None = None
        self._response_lock = RLock()

        self.startsfen = DEFAULT_INITIAL_SFEN
        self.moves: List[str] = []

        self.options: Dict[str, Tuple[str, str, Callable[[Any], str], Callable[[str], Any]]] = {
            'Threads': ('spin default {} min 1', 'threads', str, int),
            'CheckSearchDepth': ('spin default {} min 1', 'check_search_depth', str, int),
            'CheckSearchNode': ('spin default {} min 1', 'check_search_node', str, int),
            'CheckNodeDepth': ('spin default {} min 0', 'check_node_depth', str, int),
            'PucbConstantInit': ('spin default {} min 0', 'pucb_constant_init',
                                 lambda v: str(round(v * 100)), lambda s: float(s) / 100),
            'PucbConstantBase': ('spin default {} min 0', 'pucb_constant_base',
                                 lambda v: str(round(v)), lambda s: float(s)),
            'PucbMinVisitsRate': (
                'spin default {} min 0', 'pucb_min_visits_rate',
                lambda v: str(round(v * 100)), lambda s: float(s) / 100),
            'MaxVisits': ('spin default {} min 1', 'max_visits', str, int),
            'NyugyokuRule': ('combo default {} var 27 var 24', 'nyugyoku_scores',
                             lambda v: '24' if v == (31, 31) else '27',
                             lambda s: (31, 31) if s == '24' else DEFAULT_NYUGYOKU_SCORES),
            'DrawTurn': ('spin default {} min 1', 'draw_turn', str, int),
            'Visits': ('spin default {} min 1', 'visits', str, int),
            'Extends': ('spin default {} min 0', 'extends', str, int),
            'Timelimit': ('spin default {} min 0', 'timelimit',
                          lambda v: str(int(v * 1000)), lambda s: float(s) / 1000),
            'Ponder': ('check default {}', 'ponder',
                       lambda v: str(v).lower(), lambda s: s.lower() == 'true'),
            'MultiPV': ('spin default {} min 1', 'multipv', str, int),
            'Criterion': ('combo default {} var value var visits',
                          'criterion', str, str),
            'ResignThreshold': ('spin default {} min 0 max 100', 'resign_threshold',
                                lambda v: str(int(v * 100)), lambda s: float(s) / 100),
            'ResignTurn': ('spin default {} min 0', 'resign_turn', str, int),
            'InitialTurn': ('spin default {} min 0', 'initial_turn', str, int),
            'InitialWidth': ('spin default {} min 1', 'initial_width', str, int),
            'InitialTemperature': ('spin default {} min 0', 'initial_temperature',
                                   lambda v: str(int(v * 100)), lambda s: float(s) / 100),
            'InitialValueDelta': ('spin default {} min 0', 'initial_value_delta',
                                  lambda v: str(int(v * 100)), lambda s: float(s) / 100),
            'InitialWhiteOnly': ('check default {}', 'initial_white_only',
                                 lambda v: str(v).lower(), lambda s: s.lower() == 'true'),
        }

    def run(self) -> None:
        '''Run the USI engine.'''
        try:
            while True:
                # Read command
                command = self.reader.readline()

                # If there is no data to read (no newline character), exit
                if len(command) == 0:
                    break

                # Remove newline character
                command = command.strip()

                # If nothing was read, continue reading
                if len(command) == 0:
                    continue

                # Decode the command without waiting for the search thread.
                LOGGER.debug('USI command: %s', command)
                tokens = command.split()
                name = tokens[0].lower().replace('-', '_')

                # Exit through the shared search cleanup path.
                if name == 'quit':
                    break

                # Handle control commands while the search thread is active.
                if name in ('isready', 'stop', 'ponderhit'):
                    self._perform(command)
                    continue

                # Keep the active search running when an unknown command arrives.
                if not hasattr(self, f'_perform_command_{name}'):
                    self._perform(command)
                    continue

                # Stop the old search and suppress its response before changing state.
                self._stop_search(discard=True)
                self._join_search()

                if name == 'go':
                    # Register the request before starting its thread so early stops are accepted.
                    options = [token.lower() for token in tokens[1:]]
                    mode = (
                        'ponder' if 'ponder' in options else
                        'infinite' if 'infinite' in options else 'normal')

                    request = _SearchRequest(mode=mode)
                    with self._response_lock:
                        self._request = request

                    self.thread = Thread(target=self._perform, args=(command, request))
                    self.thread.start()
                else:
                    self._perform(command)
        except BaseException as e:
            LOGGER.error('USI error: %s', e)
        finally:
            # Suppress further responses and stop searching on quit or end of input.
            self.terminated = True
            self._stop_search(discard=True)
            self._join_search()

    def _stop_search(self, discard: bool = False) -> None:
        '''Notify the active search to stop.
        Args:
            discard (bool): True to suppress the old search response as well.
        Returns:
            None: No return value.
        '''
        # Notify only unanswered requests without restarting completed requests.
        with self._response_lock:
            request = self._request
            active = request is not None and not request.responded

            if request is not None and active:
                request.discard = request.discard or discard
                request.stop_event.set()

        # Stop background searches too when changing positions or shutting down.
        if self.player is not None and (active or discard):
            self.player.stop_evaluation()

    def _join_search(self) -> None:
        '''Wait for the search thread after requesting it to stop.
        Returns:
            None: No return value.
        '''
        if self.thread is not None:
            self.thread.join()
            self.thread = None
        with self._response_lock:
            self._request = None

    def _write_response(
        self, message: str, request: _SearchRequest | None = None,
    ) -> None:
        '''Write response lines and prevent duplicate final responses.
        Args:
            message (str): Response text, possibly containing multiple lines.
            request (_SearchRequest | None): Search request, or None for control commands.
        Returns:
            None: No return value.
        '''
        # Serialize response checks and writes against quit, completion, and stop.
        with self._response_lock:
            if request is not None:
                if request.discard or request.responded:
                    return
                if any(line.startswith(('bestmove ', 'checkmate '))
                       for line in message.splitlines()):
                    request.responded = True
            if message:
                LOGGER.debug('USI response: %s', message)
                self.writer.write(f'{message}\n')
                self.writer.flush()

    def _get_timelimit(
        self,
        remain_time: float,
        byoyomi_time: float,
        increment_time: float,
        current_moves: int,
    ) -> float:
        '''Get thinking time.
        Args:
            remain_time (float): Remaining time
            byoyomi_time (float): Byoyomi time
            increment_time (float): Increment time
            current_moves (int): Total moves already played by both sides.
        Returns:
            float: Thinking time
        '''
        # Ensure the active search draw limit is available.
        if self.player is None:
            raise ShogiException('player is not initialized')

        # Calculate thinking time from the current move count and active draw limit.
        timelimit = get_timelimit(
            remain_time=remain_time,
            byoyomi_time=byoyomi_time,
            increment_time=increment_time,
            current_moves=current_moves,
            draw_turn=self.player.referee.draw_turn)

        # Apply the thinking time cap configured for the USI engine.
        return min(self.timelimit, timelimit)

    def _perform(self, command: str, request: _SearchRequest | None = None) -> None:
        '''Execute command.
        Args:
            command (str): Command
            request (_SearchRequest | None): Search request, or None for control commands.
        '''
        while True:
            try:
                # Execute process
                if request is None:
                    stat, message, cont = self._perform_command(command)
                else:
                    stat, message, cont = self._perform_command_go(
                        command.split()[1:], request=request)

                # Output response
                if stat:
                    self._write_response(message, request)
                elif len(message) > 0:
                    LOGGER.error('USI error: %s', message)

                # If not continuing execution, break loop
                if not cont or (request is not None and request.discard):
                    break
            except BaseException as e:
                LOGGER.error('USI error: %s', e)
                break

    def _perform_command(self, command: str) -> Tuple[bool, str, bool]:
        '''Execute command.
        Args:
            command (str): Command string (including arguments)
        Returns:
            Tuple[bool, str, bool]: (True to output, message, True to continue execution)
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
        '''Return startup information.
        Args:
            args (List[str]): Arguments
        Returns:
            Tuple[bool, str, bool]: (True to output, message, True to continue execution)
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
        '''Set the value of an option.
        Args:
            args (List[str]): Arguments
        Returns:
            Tuple[bool, str, bool]: (True to output, message, True to continue execution)
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
        '''Return ready status.
        Args:
            args (List[str]): Arguments
        Returns:
            Tuple[bool, str, bool]: (True to output, message, True to continue execution)
        '''
        # Create player object if not already created
        if self.player is None:
            self.player = Player(
                processor=self.processor,
                threads=self.threads,
                max_visits=self.max_visits,
                nyugyoku_scores=self.nyugyoku_scores,
                check_search_depth=self.check_search_depth,
                check_search_node=self.check_search_node,
                check_node_depth=self.check_node_depth,
                pucb_constant_init=self.pucb_constant_init,
                pucb_constant_base=self.pucb_constant_base,
                pucb_min_visits_rate=self.pucb_min_visits_rate)

            # Apply the configured draw limit to the initial position.
            self.player.initialize(draw_turn=self.draw_turn)

        return (True, 'readyok', False)

    def _perform_command_usinewgame(self, args: List[str]) -> Tuple[bool, str, bool]:
        '''Start a new game.
        Args:
            args (List[str]): Command arguments.
        Returns:
            Tuple[bool, str, bool]: (Whether to output, message, whether to continue).
        '''
        # Ensure that the player has been created.
        if self.player is None:
            raise ShogiException('player is not initialized')

        # Initialize game state
        self.startsfen = DEFAULT_INITIAL_SFEN
        self.moves.clear()

        # Initialize player object
        self.player.initialize(self.startsfen, draw_turn=self.draw_turn)

        return (False, '', False)

    def _perform_command_position(self, args: List[str]) -> Tuple[bool, str, bool]:
        '''Set the board position.
        Args:
            args (List[str]): Arguments
        Returns:
            Tuple[bool, str, bool]: (True to output, message, True to continue execution)
        '''
        # Ensure that the player has been created.
        if self.player is None:
            raise ShogiException('player is not initialized')

        # Get initial position
        index = 0

        if index < len(args) and args[index].lower() == 'startpos':
            sfen = DEFAULT_INITIAL_SFEN
            index += 1
        elif index + 1 < len(args) and args[index].lower() == 'sfen':
            sfen = ' '.join(args[index + 1:index + 5])
            index += 5
        else:
            return (False, 'syntax error', False)

        # Get move list
        if index < len(args) and args[index].lower() == 'moves':
            moves = args[index + 1:]
        else:
            moves = []

        # If initial position and move history match, keep only necessary moves
        if sfen == self.startsfen and moves[:len(self.moves)] == self.moves:
            moves = moves[len(self.moves):]
        # If not matching, initialize position
        else:
            self.player.initialize(sfen, draw_turn=self.draw_turn)
            self.startsfen = sfen
            self.moves.clear()

        # Execute moves
        for move in moves:
            src, dst, promote = usi_string_to_move(move)
            self.player.play(src, dst, promote)
            self.moves.append(move)

        # Output log
        if LOGGER.isEnabledFor(logging.DEBUG):
            LOGGER.debug(
                'Position: sfen=%s, moves=%s\n%s',
                sfen, ','.join(moves), self.player.get_board())

        return (False, '', False)

    def _perform_command_go(
        self,
        args: List[str],
        analyze: bool = True,
        request: _SearchRequest | None = None,
    ) -> Tuple[bool, str, bool]:
        '''Start thinking.
        Args:
            args (List[str]): Argument list
            analyze (bool): True to also output analysis results
            request (_SearchRequest | None): Search request carrying stop and response state.
        Returns:
            Tuple[bool, str, bool]: (True to output, message, True to continue execution)
        '''
        # Ensure that the player has been created.
        if self.player is None:
            raise ShogiException('player is not initialized')

        # Parse arguments
        index = 0
        ponder = False
        btime: float | None = None
        wtime: float | None = None
        byoyomi: float | None = None
        binc: float | None = None
        winc: float | None = None

        while index < len(args):
            arg = args[index].lower()

            if arg in ('ponder', 'infinite'):
                ponder = True
                index += 1
            elif arg == 'mate':
                # Do not use mate-search time as the remaining time for normal search.
                return (True, 'checkmate notimplemented', False)
            elif index + 1 < len(args) and arg == 'btime':
                btime = float(args[index + 1]) / 1000
                index += 2
            elif index + 1 < len(args) and arg == 'wtime':
                wtime = float(args[index + 1]) / 1000
                index += 2
            elif index + 1 < len(args) and arg == 'byoyomi':
                byoyomi = float(args[index + 1]) / 1000
                index += 2
            elif index + 1 < len(args) and arg == 'binc':
                binc = float(args[index + 1]) / 1000
                index += 2
            elif index + 1 < len(args) and arg == 'winc':
                winc = float(args[index + 1]) / 1000
                index += 2
            else:
                # Skip unsupported or incomplete arguments without stalling the parser.
                index += 1

        # Use the default time limit when remaining time is unspecified.
        board = self.player.get_board()
        color = self.player.get_color()
        remain_time = btime if color == COLOR_BLACK else wtime
        increment_time = binc if color == COLOR_BLACK else winc

        # If ponder is specified in arguments, set time limit to 0.5 seconds
        if ponder:
            # Use a 0.5-second update interval for pondering and infinite searches.
            timelimit = 0.5
        elif remain_time is not None or byoyomi is not None or increment_time is not None:
            # Default omitted time fields to zero and honor explicitly specified zeroes.
            timelimit = self._get_timelimit(
                remain_time=remain_time if remain_time is not None else 0.0,
                byoyomi_time=byoyomi if byoyomi is not None else 0.0,
                increment_time=increment_time if increment_time is not None else 0.0,
                current_moves=board.get_turn())
        else:
            # Use the configured cap when no time fields are provided.
            timelimit = self.timelimit

        # Check the game state.
        if len(board.get_legal_moves()) == 0:
            # Defer terminal responses until stopped when pondering or searching infinitely.
            if ponder:
                return self._wait_terminal_response('bestmove resign', request)

            LOGGER.debug(
                'GameEnd: winner=%s',
                get_color_name(get_opposite_color(self.player.get_color())))
            return (True, 'bestmove resign', False)

        if board.is_nyugyoku():
            # Defer winning declarations too when pondering or searching infinitely.
            if ponder:
                return self._wait_terminal_response('bestmove win', request)

            LOGGER.debug(
                'GameEnd: winner=%s',
                get_color_name(self.player.get_color()))
            return (True, 'bestmove win', False)

        # If ponder is specified in arguments, set visits to max_visits
        visits = self.max_visits if ponder else self.visits

        # Record the state before starting the search.
        prev_time = time.time()
        prev_nodes = self.player.get_visits() if analyze else 0

        # Determine whether to select a random move.
        random_move = (
            not ponder and len(self.moves) < self.initial_turn
            and (not self.initial_white_only or self.player.get_color() == COLOR_WHITE))

        # Compute the move.
        if request is not None and request.stop_event.is_set():
            # Use available results without starting a new search after stopping.
            candidates = request.candidates
        elif random_move:
            LOGGER.debug('RandomMove: turn=%d', len(self.moves))
            candidates = [self.player.get_random_candidate(
                width=self.initial_width,
                timelimit=timelimit,
                temperature=self.initial_temperature,
                delta=self.initial_value_delta,
                ponder=self.ponder,
                stop_event=request.stop_event if request is not None else None)]
        else:
            LOGGER.debug(
                'Evaluate: visits=%d, timelimit=%.1f, ponder=%s',
                visits, timelimit, ponder)
            candidates = self.player.evaluate(
                visits=visits,
                extends=self.extends,
                timelimit=timelimit,
                criterion=self.criterion,
                equally=(self.multipv > 1),
                candidate_width=self.multipv if self.multipv > 1 else 0,
                ponder=self.ponder,
                stop_event=request.stop_event if request is not None else None)

        # Return a legal move rather than resigning if stopped before evaluation.
        if not candidates and request is not None and request.stop_event.is_set():
            if request.candidates:
                candidates = request.candidates
            else:
                src, dst, promote = board.get_legal_moves()[0]
                move = usi_move_to_string(src, dst, promote)
                return (True, f'bestmove {move}', False)

        # Retain the latest results in case stopping occurs before the next update.
        if request is not None:
            request.candidates = candidates

        # Raise an error if no candidates exist, indicating invalid search settings.
        if candidates[0].src == -1:
            raise ShogiException('no candidates')

        # Calculate the time spent searching.
        elapsed_time = time.time() - prev_time

        # When pondering, wait until the timeout expires.
        if ponder:
            interval = max(timelimit - elapsed_time, 0.0)

            if request is None:
                # Wait for the update interval when no stop event is available.
                time.sleep(interval)
            else:
                # Interrupt the update interval wait when stopping.
                request.stop_event.wait(interval)

        # Return results after stopping without further search or resignation checks.
        stopped = request is not None and request.stop_event.is_set()

        if stopped:
            ponder = False
            analyze = False

        # Create output
        results: List[str] = []

        # Create analysis result string
        if analyze:
            nodes = self.player.get_visits()
            nps = round((nodes - prev_nodes) / max(elapsed_time, 0.1))

            results.extend(
                usi_candidate_to_string(candidate, i + 1)
                for i, candidate in enumerate(candidates))
            results.append(f'info nodes {nodes} nps {nps}')

        # If ponder, do not return bestmove
        if ponder:
            return (True, '\n'.join(results), True)

        # If win rate is below threshold, judge as resignation
        win_chance = candidates[0].win_chance

        if (not ponder
                and not stopped
                and len(self.moves) > self.resign_turn
                and win_chance < self.resign_threshold):
            LOGGER.debug('Resign: win_chance=%.2f', win_chance)
            return (True, 'bestmove resign', False)

        # Add move
        move = usi_move_to_string(
            candidates[0].src, candidates[0].dst, candidates[0].promote)

        # Add bestmove to results
        results.append(f'bestmove {move}')

        # Output log
        if LOGGER.isEnabledFor(logging.DEBUG):
            board = Board(self.startsfen)
            board.copy_from(self.player.get_board())
            board.play(candidates[0].src, candidates[0].dst, candidates[0].promote)

            LOGGER.debug(
                'Bestmove: src=%s, dst=%s, promote=%s\n%s',
                candidates[0].src, candidates[0].dst, candidates[0].promote, board)

        return (True, '\n'.join(results), False)

    def _wait_terminal_response(
        self, message: str, request: _SearchRequest | None,
    ) -> Tuple[bool, str, bool]:
        '''Defer terminal pondering responses until a stop notification arrives.
        Args:
            message (str): Terminal response to return after stopping.
            request (_SearchRequest | None): Current search request.
        Returns:
            Tuple[bool, str, bool]: Whether to output, response text, and whether to continue.
        '''
        # Keep requests from the input loop pending until a stop command arrives.
        if request is None:
            return (False, '', False)

        # Wait for a stop notification.
        request.stop_event.wait()

        return (True, message, False)

    def _perform_command_stop(self, args: List[str]) -> Tuple[bool, str, bool]:
        '''Interrupt thinking.
        Args:
            args (List[str]): Argument list
        Returns:
            Tuple[bool, str, bool]: (True to output, message, True to continue execution)
        '''
        # Let the original search thread respond without starting another search.
        self._stop_search()

        return (False, '', False)

    def _perform_command_ponderhit(self, args: List[str]) -> Tuple[bool, str, bool]:
        '''Execute ponderhit command.
        Args:
            args (List[str]): Argument list
        Returns:
            Tuple[bool, str, bool]: (True to output, message, True to continue execution)
        '''
        # Return existing results from the same search thread when pondering hits.
        with self._response_lock:
            request = self._request
            pondering = request is not None and request.mode == 'ponder'

        # Stop the search if it is pondering.
        if pondering:
            self._stop_search()

        return (False, '', False)

    def _perform_command_gameover(self, args: List[str]) -> Tuple[bool, str, bool]:
        '''Notify game over.
        Args:
            args (List[str]): Argument list
        Returns:
            Tuple[bool, str, bool]: (True to output, message, True to continue execution)
        '''
        # Ensure that the player has been created.
        if self.player is None:
            raise ShogiException('player is not initialized')

        # Initialize player object
        self.player.initialize(draw_turn=self.draw_turn)

        return (False, '', False)

    def _perform_command_showboard(self, args: List[str]) -> Tuple[bool, str, bool]:
        '''Return the current board position.
        Args:
            args (List[str]): Arguments
        Returns:
            Tuple[bool, str, bool]: (True to output, message, True to continue execution)
        '''
        # Ensure that the player has been created.
        if self.player is None:
            raise ShogiException('player is not initialized')

        # Return the current board position
        return (True, str(self.player.get_board()), False)

    def _perform_command_play(self, args: List[str]) -> Tuple[bool, str, bool]:
        '''Execute play command.
        Args:
            args (List[str]): Argument list
        Returns:
            Tuple[bool, str, bool]: (True to output, message, True to continue execution)
        '''
        # Ensure that the player has been created.
        if self.player is None:
            raise ShogiException('player is not initialized')

        # Check arguments
        if len(args) < 1:
            return (False, 'syntax error', False)

        # Get move
        move = usi_string_to_move(args[0])

        # Execute move
        try:
            self.player.play(*move)
            self.moves.append(args[0])
        except ShogiException:
            return (False, 'illegal move', False)

        # Output log
        if LOGGER.isEnabledFor(logging.DEBUG):
            LOGGER.debug(
                'Play: src=%s, dst=%s, promote=%s\n%s',
                move[0], move[1], move[2], self.player.get_board())

        return (False, '', False)

    def _perform_command_genmove(self, args: List[str]) -> Tuple[bool, str, bool]:
        '''Execute genmove command.
        Args:
            args (List[str]): Argument list
        Returns:
            Tuple[bool, str, bool]: (True to output, message, True to continue execution)
        '''
        # Ensure that the player has been created.
        if self.player is None:
            raise ShogiException('player is not initialized')

        # Compute the move.
        _, message, _ = self._perform_command_go([], analyze=False)
        move = message.split()[1]

        # Parse the move.
        # Return unparseable moves unchanged, such as bestmove resign.
        try:
            src, dst, promote = usi_string_to_move(move)
        except ShogiException:
            return (True, message, False)

        # Advance the position.
        self.player.play(src, dst, promote)
        self.moves.append(move)

        # Return move
        return (True, message, False)
