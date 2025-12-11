import logging
import sys
from threading import Thread
from typing import Any, Callable, Dict, List, TextIO, Tuple

from deepshogi.board import Board

from .config import (AUTHOR, BOARD_SIZE, COLOR_BLACK, COLOR_WHITE,
                     DEFAULT_DRAW_TURN, DEFAULT_INITIAL_SFEN,
                     DEFAULT_NYUGYOKU_SCORES, NAME, PIECE_HAND_BISHOP,
                     PIECE_HAND_GOLD, PIECE_HAND_KNIGHT, PIECE_HAND_LANCE,
                     PIECE_HAND_PAWN, PIECE_HAND_ROOK, PIECE_HAND_SILVER,
                     VERSION, get_color_name, get_opposite_color,
                     get_shogi_score)
from .exception import ShogiException
from .player import Candidate, Player
from .processor import Processor

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
        Tuple[Tuple[int,int], Tuple[int,int],bool]: (source coordinates, destination coordinates, True if promote)
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


def usi_candidate_to_string(candidate: Candidate, criterion: str, index: int) -> str:
    '''Convert candidate move information to a USI analysis result string.
    Args:
        candidate (Candidate): Candidate move information
        criterion (str): Criterion ('value', 'minimax', or 'visits')
        index (int): Priority
    Returns:
        str: USI analysis result string
    '''
    nodes = candidate.visits
    score = get_shogi_score(candidate.get_win_chance(criterion))
    text = f'info multipv {index} nodes {nodes} score cp {score}'

    if len(candidate.variations) > 0:
        pv = ' '.join(usi_move_to_string(s, d, p) for s, d, p in candidate.variations)
        text += f' pv {pv}'

    return text


class USIEngine(object):
    '''Class that executes thinking process based on USI protocol input.'''

    def __init__(
        self,
        processor: Processor,
        threads: int,
        visits: int,
        playouts: int = 0,
        timelimit: float = 0,
        algorithm: str = 'pucb',
        criterion: str = 'value',
        ponder: bool = False,
        resign_threshold: float = 0.0,
        resign_turn: int = 0,
        initial_turn: int = 4,
        initial_width: int = 16,
        nyugyoku_scores: Tuple[int, int] = DEFAULT_NYUGYOKU_SCORES,
        draw_turn: int = DEFAULT_DRAW_TURN,
        check_search_depth: int = 31,
        check_search_node: int = 10_000,
        check_node_depth: int = 2,
        client_name: str = NAME,
        client_version: str = VERSION,
        client_author: str = AUTHOR,
        reader: TextIO = sys.stdin,
        writer: TextIO = sys.stdout,
    ):
        '''Create a USI engine.
        Args:
            processor (Processor): Inference execution object
            threads (int): Number of threads to use
            visits (int): Number of search visits
            playouts (int): Number of search playouts
            timelimit (float): Thinking time limit (seconds)
            algorithm (str): Search algorithm ('ucb' or 'pucb')
            criterion (str): Criterion for prioritizing candidate moves ('value', 'minimax', or 'visits')
            ponder (bool): True to continue analysis during opponent's thinking
            resign_threshold (float): Win rate threshold for resignation
            resign_turn (int): Minimum number of turns before resignation
            initial_turn (int): Number of initial turns for random moves
            initial_width (int): Number of candidate moves for random moves
            nyugyoku_scores (Tuple[int,int]): Points required for nyugyoku declaration
            draw_turn (int): Number of turns for a draw
            check_search_depth (int): Depth for checkmate search
            check_search_node (int): Number of nodes for checkmate search
            check_node_depth (int): Depth of nodes for checkmate search
            max_visits (int): Maximum number of search visits
            client_name (str): Client display name
            client_version (str): Client display version
            client_author (str): Displayed author name
            reader (TextIO): Stream to input commands
            writer (TextIO): Stream to output results
        '''
        self.player: Player | None = None
        self.processor = processor
        self.threads = threads
        self.nyugyoku_scores = nyugyoku_scores
        self.draw_turn = draw_turn
        self.check_search_depth = check_search_depth
        self.check_search_node = check_search_node
        self.check_node_depth = check_node_depth

        self.visits = visits
        self.playouts = playouts
        self.timelimit = timelimit
        self.algorithm = algorithm
        self.criterion = criterion
        self.ponder = ponder

        self.resign_threshold = resign_threshold
        self.resign_turn = resign_turn

        self.initial_turn = initial_turn
        self.initial_width = initial_width

        self.client_name = client_name
        self.client_version = client_version
        self.client_author = client_author

        self.reader = reader
        self.writer = writer

        self.thread: Thread | None = None
        self.terminated = False

        self.startsfen = DEFAULT_INITIAL_SFEN
        self.moves: List[str] = []

        self.options: Dict[str, Tuple[str, str, Callable[[Any], str], Callable[[str], Any]]] = {
            'Threads': ('spin default {} min 1', 'threads', str, int),
            'CheckSearchDepth': ('spin default {} min 1', 'check_search_depth', str, int),
            'CheckSearchNode': ('spin default {} min 1', 'check_search_node', str, int),
            'CheckNodeDepth': ('spin default {} min 1', 'check_node_depth', str, int),
            'NyugyokuRule': ('combo default {} var 27 var 24', 'nyugyoku_scores',
                             lambda v: '24' if v == (31, 31) else '27',
                             lambda s: (31, 31) if s == '24' else DEFAULT_NYUGYOKU_SCORES),
            'DrawTurn': ('spin default {} min 1', 'draw_turn', str, int),
            'Visits': ('spin default {} min 1', 'visits', str, int),
            'Playouts': ('spin default {} min 0', 'playouts', str, int),
            'Timelimit': ('spin default {} min 0', 'timelimit',
                          lambda v: str(int(v * 1000)), lambda s: float(s) / 1000),
            'Ponder': ('check default {}', 'ponder',
                       lambda v: str(v).lower(), lambda s: s.lower() == 'true'),
            'Algorithm': ('combo default {} var ucb var pucb', 'algorithm', str, str),
            'Criterion': ('combo default {} var value var minimax var visits',
                          'criterion', str, str),
            'ResignThreshold': ('spin default {} min 0 max 100', 'resign_threshold',
                                lambda v: str(int(v * 100)), lambda s: float(s) / 100),
            'ResignTurn': ('spin default {} min 0', 'resign_turn', str, int),
            'InitialTurn': ('spin default {} min 0', 'initial_turn', str, int),
        }

    def run(self) -> None:
        '''Run the USI engine.'''
        while True:
            try:
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

                # If a thread is running, terminate it
                if self.thread is not None:
                    self.terminated = True
                    self.thread.join()

                # Output log
                LOGGER.debug('USI command: %s', command)

                # If quit command, break loop
                if command.lower().startswith('quit'):
                    self.thread = None
                    break

                # Execute command in a separate thread
                self.terminated = False
                self.thread = Thread(target=self._perform, args=(command,))
                self.thread.start()
            except BaseException as e:
                LOGGER.error('USI error: %s', e)
                break

    def _get_timelimit(
        self,
        remain_time: float = 0x7fffffff,
        byoyomi_time: float = 0.0,
        increment_time: float = 0.0,
    ) -> float:
        '''Get thinking time.
        Args:
            remain_time (float): Remaining time
            byoyomi_time (float): Byoyomi time
            increment_time (float): Increment time
        Returns:
            float: Thinking time
        '''
        each_time = byoyomi_time + increment_time
        margin_time = 20.0 if each_time < 2.0 else 0.0
        timelimit = max(remain_time - margin_time, 0.0) * 0.1
        timelimit += max(each_time - 2.0, 0.0)

        return min(self.timelimit, timelimit)

    def _perform(self, command: str) -> None:
        '''Execute command.
        Args:
            command (str): Command
        '''
        while True:
            try:
                # Execute process
                stat, message, cont = self._perform_command(command)

                # Output response
                if stat:
                    LOGGER.debug('USI response: %s', message)
                    self.writer.write(f'{message}\n')
                    self.writer.flush()
                elif len(message) > 0:
                    LOGGER.error('USI error: %s', message)

                # If not continuing execution, break loop
                if not cont or self.terminated:
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
                nyugyoku_scores=self.nyugyoku_scores,
                draw_turn=self.draw_turn,
                check_search_depth=self.check_search_depth,
                check_search_node=self.check_search_node)

        return (True, 'readyok', False)

    def _perform_command_usinewgame(self, args: List[str]) -> Tuple[bool, str, bool]:
        '''Start a new game.
        Args:
            args (List[str]): Arguments
        Returns:
            Tuple[bool, str, bool]: (True to output, message, True to continue execution)
        '''
        # Ensure player object is created
        if self.player is None:
            raise ShogiException('player is not initialized')

        # Initialize game state
        self.startsfen = DEFAULT_INITIAL_SFEN
        self.moves.clear()

        # Initialize player object
        self.player.initialize(self.startsfen)

        return (False, '', False)

    def _perform_command_position(self, args: List[str]) -> Tuple[bool, str, bool]:
        '''Set the board position.
        Args:
            args (List[str]): Arguments
        Returns:
            Tuple[bool, str, bool]: (True to output, message, True to continue execution)
        '''
        # Ensure player object is created
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
            self.player.initialize(sfen)
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
    ) -> Tuple[bool, str, bool]:
        '''Start thinking.
        Args:
            args (List[str]): Argument list
            analyze (bool): True to also output analysis results
        Returns:
            Tuple[bool, str, bool]: (True to output, message, True to continue execution)
        '''
        # Ensure player object is created
        if self.player is None:
            raise ShogiException('player is not initialized')

        # Parse arguments
        index = 0
        ponder = False
        btime = -1.0
        wtime = -1.0
        byoyomi = 0.0
        binc = 0.0
        winc = 0.0

        while index < len(args):
            if args[index].lower() == 'ponder':
                ponder = True
                index += 1
            elif args[index].lower() == 'infinite':
                ponder = True
                index += 1
            elif index + 1 < len(args) and args[index].lower() == 'btime':
                btime = float(args[index + 1]) / 1000
                index += 2
            elif index + 1 < len(args) and args[index].lower() == 'wtime':
                wtime = float(args[index + 1]) / 1000
                index += 2
            elif index + 1 < len(args) and args[index].lower() == 'byoyomi':
                byoyomi = float(args[index + 1]) / 1000
                index += 2
            elif index + 1 < len(args) and args[index].lower() == 'binc':
                binc = float(args[index + 1]) / 1000
                index += 2
            elif index + 1 < len(args) and args[index].lower() == 'winc':
                winc = float(args[index + 1]) / 1000
                index += 2
            elif index + 1 < len(args) and args[index].lower() == 'mate':
                if args[index + 1].lower() != 'infinite':
                    btime = float(args[index + 1]) / 1000
                    wtime = float(args[index + 1]) / 1000
                index += 2

        # Calculate time limit
        # If remaining time is not set, use default time limit
        # If ponder is specified in arguments, set time limit to 0.5 seconds
        if ponder:
            timelimit = 0.5
        elif self.player.get_color() == COLOR_BLACK and btime >= 0:
            timelimit = self._get_timelimit(btime, byoyomi, binc)
        elif self.player.get_color() == COLOR_WHITE and wtime >= 0:
            timelimit = self._get_timelimit(wtime, byoyomi, winc)
        else:
            timelimit = self._get_timelimit()

        # Check game state
        board = self.player.get_board()

        if len(board.get_legal_moves()) == 0:
            LOGGER.debug(
                'GameEnd: winner=%s',
                get_color_name(get_opposite_color(self.player.get_color())))
            return (True, 'bestmove resign', False)

        if board.is_nyugyoku():
            LOGGER.debug(
                'GameEnd: winner=%s',
                get_color_name(self.player.get_color()))
            return (True, 'bestmove win', False)

        # If ponder is specified in arguments, set visits to 100,000,000
        visits = 100_000_000 if ponder else self.visits

        # Calculate move
        # If it's the initial turn and ponder is not specified, make a random move
        # Otherwise, calculate the move using the evaluation function
        if not ponder and len(self.moves) < self.initial_turn:
            LOGGER.debug('RandomMove: turn=%d', len(self.moves))
            candidates = [self.player.get_random(width=self.initial_width, timelimit=timelimit)]
        else:
            LOGGER.debug(
                'Evaluate: visits=%d, playouts=%d, timelimit=%.1f, ponder=%s',
                visits, self.playouts, timelimit, ponder)
            candidates = self.player.evaluate(
                visits=visits,
                playouts=self.playouts,
                timelimit=timelimit,
                algorithm=self.algorithm,
                criterion=self.criterion,
                check_node_depth=self.check_node_depth,
                ponder=self.ponder)

        # If there are no candidate moves, raise an exception (only occurs if
        # search settings are incorrect)
        if candidates[0].src == -1:
            raise ShogiException('no candidates')

        # Create output
        results: List[str] = []

        # Create analysis result string
        if analyze:
            results.extend(
                usi_candidate_to_string(candidate, self.criterion, i + 1)
                for i, candidate in enumerate(candidates))

        # If ponder, do not return bestmove
        if ponder:
            return (True, '\n'.join(results), True)

        # If win rate is below threshold, judge as resignation
        win_chance = candidates[0].get_win_chance(self.criterion)

        if (not ponder
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

    def _perform_command_stop(self, args: List[str]) -> Tuple[bool, str, bool]:
        '''Interrupt thinking.
        Args:
            args (List[str]): Argument list
        Returns:
            Tuple[bool, str, bool]: (True to output, message, True to continue execution)
        '''
        return self._perform_command_go(['btime', '0', 'wtime', '0'], analyze=False)

    def _perform_command_ponderhit(self, args: List[str]) -> Tuple[bool, str, bool]:
        '''Execute ponderhit command.
        Args:
            args (List[str]): Argument list
        Returns:
            Tuple[bool, str, bool]: (True to output, message, True to continue execution)
        '''
        return self._perform_command_go(['btime', '0', 'wtime', '0'], analyze=False)

    def _perform_command_gameover(self, args: List[str]) -> Tuple[bool, str, bool]:
        '''Notify game over.
        Args:
            args (List[str]): Argument list
        Returns:
            Tuple[bool, str, bool]: (True to output, message, True to continue execution)
        '''
        # Ensure player object is created
        if self.player is None:
            raise ShogiException('player is not initialized')

        # Initialize player object
        self.player.initialize()

        return (False, '', False)

    def _perform_command_showboard(self, args: List[str]) -> Tuple[bool, str, bool]:
        '''Return the current board position.
        Args:
            args (List[str]): Arguments
        Returns:
            Tuple[bool, str, bool]: (True to output, message, True to continue execution)
        '''
        # Ensure player object is created
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
        # Ensure player object is created
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
        # Ensure player object is created
        if self.player is None:
            raise ShogiException('player is not initialized')

        # Calculate move
        _, message, _ = self._perform_command_go([], analyze=False)
        move = message.split()[1]
        src, dst, promote = usi_string_to_move(move)

        # Advance the board
        self.player.play(src, dst, promote)
        self.moves.append(move)

        # Return move
        return (True, message, False)
