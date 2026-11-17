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
from .validation import validate_positive_float, validate_positive_int

LOGGER = logging.getLogger(__name__)

USI_INFO_INTERVAL = 1.0

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


def usi_candidate_to_string(
    candidate: Candidate, index: int, elapsed_ms: int, nodes: int, nps: int,
) -> str:
    '''Convert candidate information into a USI analysis string.
    Args:
        candidate (Candidate): Selected candidate move.
        index (int): Candidate priority.
        elapsed_ms (int): Milliseconds since this search began.
        nodes (int): Root visits added during this search.
        nps (int): Average visits per second.
    Returns:
        str: USI response string.
    '''
    # Attach the same search statistics to every principal variation.
    score = get_shogi_score(candidate.win_chance)
    text = (
        f'info multipv {index} time {elapsed_ms} nodes {nodes} nps {nps}'
        f' score cp {score}')

    # Report MCTS depth as the principal variation length, including the candidate itself.
    if len(candidate.variations) > 0:
        depth = len(candidate.variations)
        pv = ' '.join(usi_move_to_string(s, d, p) for s, d, p in candidate.variations)
        text += f' depth {depth} pv {pv}'

    return text


@dataclass(frozen=True)
class _PlayerSettings:
    '''Hold the settings used to construct a player.
    Args:
        threads (int): Number of search threads.
        max_visits (int): Maximum node visit count.
        nyugyoku_scores (Tuple[int, int]): Points required for entering-king declarations.
        check_search_depth (int): Checkmate search depth.
        check_search_node (int): Checkmate node capacity.
        check_node_depth (int): Node depth at which to search for checkmate.
        pucb_constant_init (float): Initial PUCB coefficient.
        pucb_constant_base (float): PUCB base; must remain finite and positive as a 32-bit float.
        pucb_min_visits_rate (float): Minimum visit ratio for prioritizing PUCB children.
    '''

    threads: int
    max_visits: int
    nyugyoku_scores: Tuple[int, int]
    check_search_depth: int
    check_search_node: int
    check_node_depth: int
    pucb_constant_init: float
    pucb_constant_base: float
    pucb_min_visits_rate: float


@dataclass(frozen=True)
class _SearchSettings:
    '''Hold the settings used for one go request.
    Args:
        visits (int): Target node visit count.
        extends (int): Maximum number of search extensions.
        timelimit (float): Time limit in seconds, including extensions.
        criterion (str): Candidate ranking criterion: 'value' or 'visits'.
        ponder (bool): True to allow GUI pondering and predicted replies.
        multipv (int): Number of candidate variations to report.
        resign_threshold (float): Win probability threshold for resignation.
        resign_turn (int): Minimum move count before resignation.
        initial_turn (int): Number of initial turns using random moves.
        initial_width (int): Number of candidates for initial random moves.
        initial_temperature (float): Temperature for initial random moves.
        initial_value_delta (float): Acceptable win probability difference for initial random moves.
        initial_white_only (bool): True to use initial random moves only for White.
        max_visits (int): Maximum node visit count.
    '''

    visits: int
    extends: int
    timelimit: float
    criterion: str
    ponder: bool
    multipv: int
    resign_threshold: float
    resign_turn: int
    initial_turn: int
    initial_width: int
    initial_temperature: float
    initial_value_delta: float
    initial_white_only: bool
    max_visits: int


@dataclass
class _SearchRequest:
    '''Hold control state and search progress shared across updates for one go request.
    Args:
        mode (str): Search mode: normal, ponder, or infinite.
        settings (_SearchSettings | None): Search settings fixed when the go request was received.
        stop_event (Event): Stop notification for this search.
        stop_reason (str | None): Stop reason: stop, ponderhit, or None before notification.
        discard (bool): True to suppress output for this request.
        responded (bool): True after the final response has been sent.
        candidates (List[Candidate]): Latest candidates; immutable after publication.
        started_at (float | None): Monotonic start time, or None before starting.
        initial_visits (int): Root visits at the start of the search.
        info_finished (Event): Notification to end periodic reporting.
        info_thread (Thread | None): Periodic reporting thread for this request.
    '''

    mode: str = 'normal'
    settings: _SearchSettings | None = None
    stop_event: Event = field(default_factory=Event)
    stop_reason: str | None = None
    discard: bool = False
    responded: bool = False
    candidates: List[Candidate] = field(default_factory=list)
    started_at: float | None = None
    initial_visits: int = 0
    info_finished: Event = field(default_factory=Event)
    info_thread: Thread | None = None


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
            threads (int): Number of search threads.
            visits (int): Target node visit count.
            extends (int): Maximum number of search extensions.
            timelimit (float): Time limit in seconds, including extensions.
            criterion (str): Candidate ranking criterion: 'value' or 'visits'.
            ponder (bool): True to allow GUI pondering and predicted replies.
            multipv (int): Number of candidate variations to report.
            max_visits (int): Maximum node visit count.
            resign_threshold (float): Win probability threshold for resignation.
            resign_turn (int): Minimum move count before resignation.
            initial_turn (int): Number of initial turns using random moves.
            initial_width (int): Number of candidates for initial random moves.
            initial_temperature (float): Temperature for initial random moves.
            initial_value_delta (float): Acceptable win probability difference for initial random
                moves.
            initial_white_only (bool): True to use initial random moves only for White.
            nyugyoku_scores (Tuple[int,int]): Points required for entering-king declarations.
            draw_turn (int): Move count at which the game is drawn.
            check_search_depth (int): Checkmate search depth.
            check_search_node (int): Checkmate node capacity.
            check_node_depth (int): Node depth at which to search for checkmate.
            pucb_constant_init (float): Initial PUCB coefficient.
            pucb_constant_base (float): Integer PUCB base of at least one, finite as a 32-bit
                float.
            pucb_min_visits_rate (float): Minimum visit ratio for prioritizing PUCB children.
            client_name (str): Displayed client name.
            client_version (str): Displayed client version.
            client_author (str): Displayed author name.
            reader (TextIO): Command input stream.
            writer (TextIO): Result output stream.
        '''
        # Validate search settings at construction rather than deferring errors until readiness.
        validate_positive_int('threads', threads)
        validate_positive_int('max_visits', max_visits)
        validate_positive_float('pucb_constant_base', pucb_constant_base)

        # Apply the same integer restrictions to spin defaults and input values.
        if not float(pucb_constant_base).is_integer():
            raise ValueError('pucb_constant_base must be an integer greater than zero.')

        self.player: Player | None = None
        self._player_settings: _PlayerSettings | None = None
        self._game_in_progress = False
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
            'Threads': ('spin default {} min 1 max 2147483647', 'threads', str, int),
            'CheckSearchDepth': (
                'spin default {} min 1 max 2147483647', 'check_search_depth', str, int),
            'CheckSearchNode': (
                'spin default {} min 1 max 2147483647', 'check_search_node', str, int),
            'CheckNodeDepth': (
                'spin default {} min 0 max 2147483647', 'check_node_depth', str, int),
            'PucbConstantInit': ('spin default {} min 0 max 2147483647', 'pucb_constant_init',
                                 lambda v: str(round(v * 100)), lambda s: float(s) / 100),
            'PucbConstantBase': ('spin default {} min 1 max 2147483647', 'pucb_constant_base',
                                 lambda v: str(round(v)), int),
            'PucbMinVisitsRate': (
                'spin default {} min 0 max 2147483647', 'pucb_min_visits_rate',
                lambda v: str(round(v * 100)), lambda s: float(s) / 100),
            'MaxVisits': ('spin default {} min 1 max 2147483647', 'max_visits', str, int),
            'NyugyokuRule': ('combo default {} var 27 var 24', 'nyugyoku_scores',
                             lambda v: '24' if v == (31, 31) else '27',
                             lambda s: (31, 31) if s == '24' else DEFAULT_NYUGYOKU_SCORES),
            # Board stores the draw move count as a signed 16-bit integer.
            'DrawTurn': ('spin default {} min 1 max 32767', 'draw_turn', str, int),
            'Visits': ('spin default {} min 1 max 2147483647', 'visits', str, int),
            'Extends': ('spin default {} min 0 max 2147483647', 'extends', str, int),
            # Limit seconds so conversion to int32_t milliseconds cannot overflow.
            'Timelimit': ('spin default {} min 0 max 2147483500', 'timelimit',
                          lambda v: str(int(v * 1000)), lambda s: float(s) / 1000),
            'Ponder': ('check default {}', 'ponder',
                       lambda v: str(v).lower(), lambda s: s.lower() == 'true'),
            'MultiPV': ('spin default {} min 1 max 2147483647', 'multipv', str, int),
            'Criterion': ('combo default {} var value var visits',
                          'criterion', str, str),
            'ResignThreshold': ('spin default {} min 0 max 100', 'resign_threshold',
                                lambda v: str(int(v * 100)), lambda s: float(s) / 100),
            'ResignTurn': ('spin default {} min 0 max 2147483647', 'resign_turn', str, int),
            'InitialTurn': ('spin default {} min 0 max 2147483647', 'initial_turn', str, int),
            # Random move evaluation passes width + 1 to a native int32_t argument.
            'InitialWidth': ('spin default {} min 1 max 2147483646', 'initial_width', str, int),
            'InitialTemperature': ('spin default {} min 0 max 2147483647', 'initial_temperature',
                                   lambda v: str(int(v * 100)), lambda s: float(s) / 100),
            'InitialValueDelta': ('spin default {} min 0 max 2147483647', 'initial_value_delta',
                                  lambda v: str(int(v * 100)), lambda s: float(s) / 100),
            'InitialWhiteOnly': ('check default {}', 'initial_white_only',
                                 lambda v: str(v).lower(), lambda s: s.lower() == 'true'),
        }

    def run(self) -> None:
        '''Process search requests and control commands while continuing to accept input.
        '''
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

                # Store settings, report readiness, and signal stops without joining the search
                # thread.
                if name in ('setoption', 'isready', 'stop', 'ponderhit'):
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

                    # Fix settings before reading more input and prevent player recreation on
                    # readiness.
                    request = _SearchRequest(mode=mode, settings=self._get_search_settings())
                    if self.player is not None:
                        self._game_in_progress = True
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

    def _stop_search(self, discard: bool = False, reason: str = 'stop') -> None:
        '''Signal the current search to stop.
        Args:
            discard (bool): True to suppress output for this request.
            reason (str): Stop reason: stop or ponderhit.
        '''
        # Signal only requests awaiting a response without restarting completed requests.
        with self._response_lock:
            request = self._request
            active = request is not None and not request.responded

            # Apply ponderhit only to ponder requests that have not responded.
            if reason == 'ponderhit' and (
                    request is None or request.responded or request.mode != 'ponder'):
                return

            if request is not None and active:
                request.discard = request.discard or discard

                # Prioritize explicit stop so a later ponderhit cannot restore a ponder request.
                if reason == 'stop' or request.stop_reason is None:
                    request.stop_reason = reason

                request.stop_event.set()

        # Stop background searches too when changing positions or shutting down.
        if self.player is not None and (active or discard):
            self.player.stop_evaluation()

    def _join_search(self) -> None:
        '''Wait for the search thread after requesting it to stop.
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
        '''
        # Serialize response checks and writes against quit, completion, and stop.
        with self._response_lock:
            if request is not None:
                if request.discard or request.responded:
                    return

                # Suppress predicted replies if stop arrives after formatting but before sending.
                if request.stop_reason == 'stop':
                    message = '\n'.join(
                        line.split(' ponder ', 1)[0] if line.startswith('bestmove ') else line
                        for line in message.splitlines())

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
        configured_timelimit: float,
    ) -> float:
        '''Calculate thinking time using active game rules and the configured limit.
        Args:
            remain_time (float): Remaining seconds before the current increment.
            byoyomi_time (float): Byoyomi time in seconds.
            increment_time (float): Increment available for the current move, in seconds.
            current_moves (int): Total moves played by both sides.
            configured_timelimit (float): Configured time limit for this search, in seconds.
        Returns:
            float: Thinking time in seconds after applying the configured limit.
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

        # Apply the USI engine's configured time limit.
        return min(configured_timelimit, timelimit)

    def _start_info(self, request: _SearchRequest, periodic: bool) -> None:
        '''Save initial statistics and start periodic reporting when requested.
        Args:
            request (_SearchRequest): Current search request, or None for a direct command.
            periodic (bool): True to start a periodic reporting thread.
        '''
        # Preserve the original baseline across ponder updates.
        if request.started_at is not None:
            return

        if self.player is None:
            raise ShogiException('player is not initialized')

        request.initial_visits = self.player.get_visits()
        request.started_at = time.monotonic()

        # Report asynchronously only for requests owned by the input loop.
        if periodic:
            info_thread = Thread(target=self._report_info, args=(request,))
            info_thread.start()
            request.info_thread = info_thread

    def _finish_info(self, request: _SearchRequest) -> None:
        '''Stop periodic reporting and join its thread before the final response.
        Args:
            request (_SearchRequest): Current search request, or None for a direct command.
        '''
        # Join without holding the response lock to avoid deadlocks with reporting output.
        request.info_finished.set()

        if request.info_thread is not None:
            request.info_thread.join()
            request.info_thread = None

    def _format_info(self, request: _SearchRequest) -> str:
        '''Format search information from the latest candidates and current statistics.
        Callers synchronize candidate updates with periodic reporting.
        Args:
            request (_SearchRequest): Current search request, or None for a direct command.
        Returns:
            str: USI response string.
        '''
        if self.player is None or request.started_at is None:
            return ''

        # Exclude existing tree visits and attach statistics from the same instant to every line.
        nodes = max(self.player.get_visits() - request.initial_visits, 0)
        elapsed = max(time.monotonic() - request.started_at, 0.0)
        elapsed_ms = int(elapsed * 1000)
        nps = round(nodes / elapsed) if elapsed > 0.0 else 0

        results = [
            usi_candidate_to_string(candidate, i + 1, elapsed_ms, nodes, nps)
            for i, candidate in enumerate(request.candidates)]
        results.append(f'info nodes {nodes} nps {nps} time {elapsed_ms}')

        return '\n'.join(results)

    def _report_info(self, request: _SearchRequest) -> None:
        '''Periodically report the latest information without performing a search.
        Args:
            request (_SearchRequest): Current search request, or None for a direct command.
        '''
        try:
            while not request.info_finished.wait(USI_INFO_INTERVAL):
                # Serialize candidate publication, request discard, and output with the same lock.
                with self._response_lock:
                    if (request.info_finished.is_set() or request.stop_event.is_set()
                            or request.discard or request.responded):
                        break

                    self._write_response(self._format_info(request), request)
        except BaseException as e:
            # Do not restart search after a reporting failure; let the search thread join this
            # thread.
            LOGGER.error('USI info error: %s', e)

    def _perform(self, command: str, request: _SearchRequest | None = None) -> None:
        '''Execute a USI command.
        Args:
            command (str): USI command.
            request (_SearchRequest | None): Current search request, or None for a direct command.
        '''
        try:
            while True:
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
        finally:
            # Stop periodic reporting even when requests are discarded or commands fail.
            if request is not None:
                self._finish_info(request)

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

        # Map the GUI's standard option name to the existing ponder setting.
        if name == 'USI_Ponder':
            name = 'Ponder'

        # Accept the hash size option without changing settings or the player.
        if name == 'USI_Hash':
            return (False, '', False)

        if name not in self.options:
            return (False, f'unknown option: {name}', False)

        _, var, _, func = self.options[name]

        # Keep existing settings unchanged until validation succeeds.
        try:
            new_value = func(value)
            if var in ('threads', 'max_visits'):
                validate_positive_int(var, new_value)
            elif var == 'pucb_constant_base':
                validate_positive_float(var, new_value)
        except ValueError as error:
            return (False, str(error), False)

        setattr(self, var, new_value)

        return (False, '', False)

    def _get_player_settings(self) -> _PlayerSettings:
        '''Return the saved player construction settings.
        Returns:
            _PlayerSettings: Latest settings for constructing a player.
        '''
        # Use the same values for comparison and construction, including reverted changes.
        return _PlayerSettings(
            threads=self.threads,
            max_visits=self.max_visits,
            nyugyoku_scores=self.nyugyoku_scores,
            check_search_depth=self.check_search_depth,
            check_search_node=self.check_search_node,
            check_node_depth=self.check_node_depth,
            pucb_constant_init=self.pucb_constant_init,
            pucb_constant_base=self.pucb_constant_base,
            pucb_min_visits_rate=self.pucb_min_visits_rate)

    def _get_search_settings(self) -> _SearchSettings:
        '''Return saved search settings and the player's active visit limit.
        Returns:
            _SearchSettings: Settings fixed for one go request.
        '''
        # Support existing callers that assign a player externally.
        max_visits = (
            self._player_settings.max_visits if self._player_settings is not None
            else self.max_visits)

        # Keep values independent of saved settings that may change during search.
        return _SearchSettings(
            visits=self.visits,
            extends=self.extends,
            timelimit=self.timelimit,
            criterion=self.criterion,
            ponder=self.ponder,
            multipv=self.multipv,
            resign_threshold=self.resign_threshold,
            resign_turn=self.resign_turn,
            initial_turn=self.initial_turn,
            initial_width=self.initial_width,
            initial_temperature=self.initial_temperature,
            initial_value_delta=self.initial_value_delta,
            initial_white_only=self.initial_white_only,
            max_visits=max_visits)

    def _perform_command_isready(self, args: List[str]) -> Tuple[bool, str, bool]:
        '''Return the engine's readiness.
        Args:
            args (List[str]): Command arguments.
        Returns:
            Tuple[bool, str, bool]: Whether to output, response text, and whether to continue.
        '''
        # Keep the player and active settings throughout a game, even while stopped.
        if self._game_in_progress:
            return (True, 'readyok', False)

        # Construct a player only if it is absent or its construction settings have changed.
        settings = self._get_player_settings()

        if self.player is None or settings != self._player_settings:
            self._stop_search(discard=True)
            self._join_search()
            player = Player(
                processor=self.processor,
                threads=settings.threads,
                max_visits=settings.max_visits,
                nyugyoku_scores=settings.nyugyoku_scores,
                check_search_depth=settings.check_search_depth,
                check_search_node=settings.check_search_node,
                check_node_depth=settings.check_node_depth,
                pucb_constant_init=settings.pucb_constant_init,
                pucb_constant_base=settings.pucb_constant_base,
                pucb_min_visits_rate=settings.pucb_min_visits_rate)

            # Preserve the previous player and active settings if initialization fails.
            player.initialize(draw_turn=self.draw_turn)
            self.player = player
            self._player_settings = settings
            self.startsfen = DEFAULT_INITIAL_SFEN
            self.moves.clear()

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

        # Initialize the player.
        self.player.initialize(self.startsfen, draw_turn=self.draw_turn)
        self._game_in_progress = True

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

        # Treat a position command as starting a game even if usinewgame was omitted.
        self._game_in_progress = True

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
        '''Run a search and append final information after periodic reporting finishes.
        Args:
            args (List[str]): Command arguments.
            analyze (bool): True to include analysis output.
            request (_SearchRequest | None): Current search request, or None for a direct command.
        Returns:
            Tuple[bool, str, bool]: Whether to output, response text, and whether to continue.
        '''
        # Use measurement state local to this call when invoked directly.
        progress = request if request is not None else _SearchRequest()
        cont = False
        try:
            stat, message, cont = self._search_go(args, analyze, request, progress)
        finally:
            # Join the reporting thread even if analysis raises an exception.
            if not cont or request is None:
                self._finish_info(progress)

        # Use periodic reporting for ongoing pondering and avoid duplicating final output.
        if not cont or request is None:
            with self._response_lock:
                if stat and analyze and not progress.discard:
                    info = self._format_info(progress)
                    message = '\n'.join(part for part in (info, message) if part)
        return (stat, message, cont)

    def _format_bestmove(
        self, candidate: Candidate, board: Board, settings: _SearchSettings,
        mode: str, request: _SearchRequest | None,
    ) -> str:
        '''Append a validated predicted reply to the selected move when available.
        Args:
            candidate (Candidate): Selected candidate move.
            board (Board): Board at the start of this search.
            settings (_SearchSettings): Search settings fixed when the go request was received.
            mode (str): Search mode: normal, ponder, or infinite.
            request (_SearchRequest | None): Current search request, or None for a direct command.
        Returns:
            str: USI response string.
        '''
        move = (candidate.src, candidate.dst, candidate.promote)
        message = f'bestmove {usi_move_to_string(*move)}'

        # Do not request further pondering after explicit stop, analysis, or disabled pondering.
        if not settings.ponder or mode == 'infinite':
            return message

        if request is not None and (
                request.discard or request.stop_reason == 'stop'
                or (request.stop_event.is_set() and request.stop_reason != 'ponderhit')):
            return message

        # Use only the selected move's own PV; reject short or inconsistent variations.
        if len(candidate.variations) < 2 or candidate.variations[0] != move:
            return message

        if move not in board.get_legal_moves():
            return message

        # Check terminal state and legality on a copy without changing player or referee history.
        next_board = Board()
        next_board.copy_from(board)
        next_board.play(*move)

        if self.player is None or self.player.referee.judge(next_board)[0]:
            return message

        reply = candidate.variations[1]

        if reply not in next_board.get_legal_moves():
            return message

        return f'{message} ponder {usi_move_to_string(*reply)}'

    def _search_go(
        self, args: List[str], analyze: bool,
        request: _SearchRequest | None, progress: _SearchRequest,
    ) -> Tuple[bool, str, bool]:
        '''Start or update the search and format its move response.
        Args:
            args (List[str]): Command arguments.
            analyze (bool): True to include analysis output.
            request (_SearchRequest | None): Current search request, or None for a direct command.
            progress (_SearchRequest): Measurement state shared by this go request.
        Returns:
            Tuple[bool, str, bool]: Whether to output, response text, and whether to continue.
        '''
        # Ensure that the player has been created.
        if self.player is None:
            raise ShogiException('player is not initialized')

        # Fix settings for direct calls too and reuse the same values across updates.
        if request is None:
            settings = self._get_search_settings()
        else:
            if request.settings is None:
                request.settings = self._get_search_settings()

            settings = request.settings

        # Treat search as starting a game even if the new-game notification was omitted.
        self._game_in_progress = True

        # Parse command arguments.
        index = 0
        mode = 'normal'
        btime: float | None = None
        wtime: float | None = None
        byoyomi: float | None = None
        binc: float | None = None
        winc: float | None = None

        while index < len(args):
            arg = args[index].lower()

            if arg in ('ponder', 'infinite'):
                # Prioritize ponder when both modes are specified, as in the input loop.
                if arg == 'ponder' or mode == 'normal':
                    mode = arg
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
                # Skip unsupported arguments and continue parsing subsequent arguments.
                index += 1

        # Defer replies for pondering and analysis, using different continuation rules.
        deferred = mode != 'normal'

        # Use the default time limit when remaining time is unspecified.
        board = self.player.get_board()
        color = self.player.get_color()
        remain_time = btime if color == COLOR_BLACK else wtime
        increment_time = binc if color == COLOR_BLACK else winc

        # Determine the thinking time limit.
        if deferred:
            # Use a 0.5-second update interval for pondering and infinite searches.
            timelimit = 0.5
        elif remain_time is not None or byoyomi is not None or increment_time is not None:
            # Default omitted time fields to zero and honor explicitly specified zeroes.
            timelimit = self._get_timelimit(
                remain_time=remain_time if remain_time is not None else 0.0,
                byoyomi_time=byoyomi if byoyomi is not None else 0.0,
                increment_time=increment_time if increment_time is not None else 0.0,
                current_moves=board.get_turn(),
                configured_timelimit=settings.timelimit)
        else:
            # Use the configured limit for normal searches without time arguments.
            timelimit = settings.timelimit

        # Check the game state.
        if len(board.get_legal_moves()) == 0:
            # Defer ponder and infinite search replies until a control command arrives.
            if deferred:
                return self._wait_terminal_response('bestmove resign', request)

            LOGGER.debug(
                'GameEnd: winner=%s',
                get_color_name(get_opposite_color(self.player.get_color())))
            return (True, 'bestmove resign', False)

        if board.is_nyugyoku():
            # Defer winning declarations at the start of ponder or infinite searches too.
            if deferred:
                return self._wait_terminal_response('bestmove win', request)

            LOGGER.debug(
                'GameEnd: winner=%s',
                get_color_name(self.player.get_color()))

            return (True, 'bestmove win', False)

        # If pondering is disabled, defer a cached result or legal move without searching.
        if mode == 'ponder' and not settings.ponder:
            if request is not None and request.candidates:
                candidate = request.candidates[0]
                move = (candidate.src, candidate.dst, candidate.promote)
            else:
                move = board.get_legal_moves()[0]

            message = f'bestmove {usi_move_to_string(*move)}'

            return self._wait_terminal_response(message, request)

        # Use the maximum visit count for pondering and analysis, and the target for normal search.
        visits = settings.max_visits if deferred else settings.visits

        # Start search measurements once and measure ponder update intervals separately.
        if analyze:
            self._start_info(progress, periodic=request is not None)

        prev_time = time.monotonic()

        # Decide whether to choose a random move.
        random_move = (
            not deferred and len(self.moves) < settings.initial_turn
            and (not settings.initial_white_only or self.player.get_color() == COLOR_WHITE))

        # Compute the move.
        if request is not None and request.stop_event.is_set():
            # Use available results without starting a new search after stopping.
            candidates = request.candidates
        elif random_move:
            LOGGER.debug('RandomMove: turn=%d', len(self.moves))
            # Choose a random candidate.
            candidates = [self.player.get_random_candidate(
                width=settings.initial_width,
                timelimit=timelimit,
                temperature=settings.initial_temperature,
                delta=settings.initial_value_delta,
                ponder=False,
                stop_event=request.stop_event if request is not None else None)]
        else:
            LOGGER.debug(
                'Evaluate: visits=%d, timelimit=%.1f, ponder=%s',
                visits, timelimit, deferred)
            # Get candidate moves.
            # Disable extensions when the total number of played moves is at most twenty.
            candidates = self.player.evaluate(
                visits=visits,
                extends=0 if board.get_turn() <= 20 else settings.extends,
                timelimit=timelimit,
                criterion=settings.criterion,
                equally=(settings.multipv > 1),
                candidate_width=settings.multipv if settings.multipv > 1 else 0,
                ponder=deferred,
                stop_event=request.stop_event if request is not None else None)

        # Return a legal move rather than resigning if stopped before evaluation.
        if not candidates and request is not None and request.stop_event.is_set():
            if request.candidates:
                candidates = request.candidates
            else:
                src, dst, promote = board.get_legal_moves()[0]
                move_text = usi_move_to_string(src, dst, promote)
                return (True, f'bestmove {move_text}', False)

        # Publish the latest result so it remains available if stopped before the next update.
        with self._response_lock:
            progress.candidates = list(candidates)

        # Raise an error if no candidates exist, indicating invalid search settings.
        if candidates[0].src == -1:
            raise ShogiException('no candidates')

        # Measure elapsed search time.
        elapsed_time = time.monotonic() - prev_time

        # Wait for the internal update interval during pondering and analysis.
        if deferred:
            interval = max(timelimit - elapsed_time, 0.0)

            if request is None:
                # Wait for the update interval when no stop event is available.
                time.sleep(interval)
            else:
                # Interrupt the update interval wait when stopping.
                request.stop_event.wait(interval)

        # After a stop, return the result without further searching or resignation checks.
        stopped = request is not None and request.stop_event.is_set()

        # Defer bestmove for pondering and analysis until a control command arrives.
        if deferred and not stopped:
            return (True, '', True)

        # Resign when the win probability is below the threshold.
        win_chance = candidates[0].win_chance

        if (not stopped
                and len(self.moves) > settings.resign_turn
                and win_chance < settings.resign_threshold):
            LOGGER.debug('Resign: win_chance=%.2f', win_chance)
            return (True, 'bestmove resign', False)

        # Validate predicted replies without advancing history; wait for the GUI's position.
        message = self._format_bestmove(candidates[0], board, settings, mode, request)

        # Output log
        if LOGGER.isEnabledFor(logging.DEBUG):
            board = Board(self.startsfen)
            board.copy_from(self.player.get_board())
            board.play(candidates[0].src, candidates[0].dst, candidates[0].promote)

            LOGGER.debug(
                'Bestmove: src=%s, dst=%s, promote=%s\n%s',
                candidates[0].src, candidates[0].dst, candidates[0].promote, board)

        return (True, message, False)

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
        '''Execute a ponderhit command.
        Args:
            args (List[str]): Command arguments.
        Returns:
            Tuple[bool, str, bool]: Whether to output, response text, and whether to continue.
        '''
        # Stop only unanswered ponder requests and record a reason that permits a predicted reply.
        self._stop_search(reason='ponderhit')

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

        # Initialize the player.
        self.player.initialize(draw_turn=self.draw_turn)
        self.startsfen = DEFAULT_INITIAL_SFEN
        self.moves.clear()
        self._game_in_progress = False

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
