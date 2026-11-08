import unittest
from queue import Queue
from threading import Condition, Event, Thread
from unittest.mock import Mock, patch

from deepshogi.board import Board
from deepshogi.config import COLOR_BLACK, COLOR_WHITE, DEFAULT_INITIAL_SFEN
from deepshogi.player import Candidate
from deepshogi.usi import USIEngine, _SearchRequest, usi_move_to_string


class UsiTest(unittest.TestCase):
    '''Verify USI time arguments and search control branches.'''

    def setUp(self) -> None:
        '''Prepare an engine with mocked searches.
        Returns:
            None: No return value.
        '''
        # Inspect search arguments without loading a model.
        self.engine = USIEngine(
            processor=Mock(), threads=1, timelimit=1000.0, initial_turn=0)
        self.player = Mock()
        self.player.referee.draw_turn = 512
        self.player.get_visits.return_value = 100
        candidate = Candidate(
            (6, 6), (6, 5), False, COLOR_BLACK, 100, 1.0, 0.0, 1.0, [])
        self.player.evaluate.return_value = [candidate]
        self.player.get_random_candidate.return_value = candidate
        self.candidates = [candidate]
        self.engine.player = self.player
        self._set_position(0)

        # Disable debug board operations when checking USI responses and time arguments.
        logger_patch = patch('deepshogi.usi.LOGGER')
        logger = logger_patch.start()
        logger.isEnabledFor.return_value = False
        self.addCleanup(logger_patch.stop)

    def _set_position(self, current_moves: int, color: int = COLOR_BLACK) -> None:
        '''Set a position with the requested move count and side to move.
        Args:
            current_moves (int): Total moves already played by both sides.
            color (int): Current side to move.
        Returns:
            None: No return value.
        '''
        # Set the actual board move count from SFEN independently of the move history.
        fields = DEFAULT_INITIAL_SFEN.split()
        fields[1] = 'b' if color == COLOR_BLACK else 'w'
        fields[3] = str(current_moves + 1)
        self.player.get_board.return_value = Board(' '.join(fields))
        self.player.get_color.return_value = color

    def test_go_time_arguments(self) -> None:
        '''Check time units, side selection, omitted values, and explicit zeroes.
        Returns:
            None: No return value.
        '''
        # Check time passed to search in seconds against fixed expected values.
        cases = [
            (COLOR_BLACK, 'btime 600000 wtime 300000', 344.0),
            (COLOR_WHITE, 'btime 600000 wtime 300000', 44.0),
            (COLOR_BLACK, 'btime 40000 wtime 50000 binc 10000 winc 20000', 49.0),
            (COLOR_WHITE, 'btime 40000 wtime 50000 binc 10000 winc 20000', 69.0),
            (COLOR_BLACK, 'btime 60000 wtime 50000 byoyomi 10000', 69.0),
            (COLOR_WHITE, 'btime 60000 wtime 50000 byoyomi 10000', 59.0),
            (COLOR_BLACK, 'byoyomi 10000', 9.0),
            (COLOR_BLACK, 'binc 10000', 9.0),
            (COLOR_WHITE, 'winc 10000', 9.0),
            (COLOR_BLACK, 'btime 0 wtime 600000', 0.0),
            (COLOR_BLACK, 'byoyomi 0', 0.0),
            (COLOR_BLACK, 'binc 0', 0.0),
            (COLOR_BLACK, '', 1000.0),
        ]
        for color, args, expected in cases:
            with self.subTest(color=color, args=args):
                self._set_position(0, color)
                stat, message, cont = self.engine._perform_command(f'go {args}')
                self.assertAlmostEqual(
                    self.player.evaluate.call_args.kwargs['timelimit'], expected)
                self.assertTrue(stat)
                self.assertIn('bestmove 7g7f', message)
                self.assertFalse(cont)

    def test_draw_turn_initialization(self) -> None:
        '''Pass the configured draw limit during preparation, resets, and game completion.
        Returns:
            None: No return value.
        '''
        # Verify that the draw limit is passed to initialization rather than construction.
        self.engine.draw_turn = 300
        self.engine.player = None
        with patch('deepshogi.usi.Player', return_value=self.player) as constructor:
            self.engine._perform_command_isready([])
        self.assertNotIn('draw_turn', constructor.call_args.kwargs)
        self.player.initialize.assert_called_once_with(draw_turn=300)

        # Preserve the configured value through all position initialization paths.
        self.player.initialize.reset_mock()
        self.engine._perform_command_usinewgame([])
        self.player.initialize.assert_called_once_with(DEFAULT_INITIAL_SFEN, draw_turn=300)
        self.player.initialize.reset_mock()
        self.engine.startsfen = ''
        self.engine._perform_command_position(['startpos'])
        self.player.initialize.assert_called_once_with(DEFAULT_INITIAL_SFEN, draw_turn=300)
        self.player.initialize.reset_mock()
        self.engine._perform_command_gameover([])
        self.player.initialize.assert_called_once_with(draw_turn=300)

    def test_active_draw_turn_and_sfen_moves(self) -> None:
        '''Use the active draw limit and SFEN move count when calculating time.
        Returns:
            None: No return value.
        '''
        # Use different engine and player limits to check the active search conditions.
        self.engine.draw_turn = 300
        self.assertEqual(self.engine.moves, [])
        for draw_turn, current_moves, expected in (
            (200, 100, 950.0),
            (201, 100, 949.0),
            (200, 197, 998.0),
            (200, 198, 999.0),
            (200, 199, 999.0),
            (200, 200, 999.0),
            (200, 201, 999.0),
        ):
            with self.subTest(draw_turn=draw_turn, current_moves=current_moves):
                self.player.referee.draw_turn = draw_turn
                self._set_position(current_moves)
                self.engine._perform_command('go btime 1000000 wtime 1000000')
                self.assertAlmostEqual(
                    self.player.evaluate.call_args.kwargs['timelimit'], expected)

    def test_timelimit_option(self) -> None:
        '''Apply the configured USI time cap, including an explicit zero-second cap.
        Returns:
            None: No return value.
        '''
        # Accept milliseconds and use the same cap for go commands without time fields.
        for milliseconds, expected in ((10000, 10.0), (0, 0.0)):
            self.engine._perform_command(f'setoption name Timelimit value {milliseconds}')
            for args in ('byoyomi 30000', ''):
                with self.subTest(milliseconds=milliseconds, args=args):
                    self.engine._perform_command(f'go {args}')
                    self.assertEqual(
                        self.player.evaluate.call_args.kwargs['timelimit'], expected)

    def test_random_move_time(self) -> None:
        '''Apply the same time limit when selecting random moves.
        Returns:
            None: No return value.
        '''
        # Honor byoyomi-only arguments when selecting random opening moves.
        self.engine.initial_turn = 4
        self.engine._perform_command('go byoyomi 10000')
        self.assertEqual(
            self.player.get_random_candidate.call_args.kwargs['timelimit'], 9.0)
        self.player.evaluate.assert_not_called()

    def test_unknown_arguments(self) -> None:
        '''Skip unknown arguments and parse subsequent time fields.
        Returns:
            None: No return value.
        '''
        # Handle uppercase commands and time fields surrounding unsupported options.
        self.engine._perform_command(
            'GO unknown depth 5 BTIME 600000 movestogo 10 byoyomi 0 trailing')
        self.assertEqual(self.player.evaluate.call_args.kwargs['timelimit'], 344.0)

    def test_ponder_and_infinite(self) -> None:
        '''Check pondering and infinite update intervals and responses after stopping.
        Returns:
            None: No return value.
        '''
        # Defer bestmove until stopped without calculating a normal time budget.
        for mode in ('ponder', 'infinite'):
            with self.subTest(mode=mode), \
                    patch('deepshogi.usi.time.sleep'), \
                    patch('deepshogi.usi.get_timelimit') as calculate:
                stat, message, cont = self.engine._perform_command(
                    f'go {mode} btime 0 wtime 0 byoyomi 0')
                calculate.assert_not_called()
                self.assertTrue(stat)
                self.assertNotIn('bestmove', message)
                self.assertTrue(cont)
                self.assertEqual(self.player.evaluate.call_args.kwargs['timelimit'], 0.5)
                self.assertEqual(
                    self.player.evaluate.call_args.kwargs['visits'], self.engine.max_visits)

            # Do not start a search for stop commands when no request is registered.
            calls = self.player.evaluate.call_count
            for command in ('stop', 'ponderhit'):
                self.assertEqual(self.engine._perform_command(command), (False, '', False))
                self.assertEqual(self.player.evaluate.call_count, calls)

    def test_terminal_ponder_waits_for_stop(self) -> None:
        '''Defer terminal bestmove responses when starting a pondering search.
        Returns:
            None: No return value.
        '''
        # Defer checkmate and entering-king responses until stop arrives.
        for mode in ('ponder', 'infinite'):
            for checkmate in (True, False):
                with self.subTest(mode=mode, checkmate=checkmate):
                    board = Mock()
                    board.get_turn.return_value = 0
                    board.get_legal_moves.return_value = [] if checkmate else [Mock()]
                    board.is_nyugyoku.return_value = not checkmate
                    self.player.get_board.return_value = board
                    self.assertEqual(
                        self.engine._perform_command(f'go {mode}'), (False, '', False))
                    self.assertEqual(
                        self.engine._perform_command('stop'), (False, '', False))
        self.player.evaluate.assert_not_called()

    def _start_protocol(self) -> None:
        '''Start the actual USI input loop with a queue as input.
        Returns:
            None: No return value.
        '''
        # Wait for responses using a condition variable instead of fixed sleeps.
        self.commands: Queue[str] = Queue()
        self.output: list[str] = []
        self.output_condition = Condition()
        self.search_started = Event()
        self.search_release = Event()
        self.stop_notified = Event()

        def write(message: str) -> int:
            '''Record message (str) and return its character count (int).'''
            with self.output_condition:
                self.output.extend(message.splitlines())
                self.output_condition.notify_all()
            return len(message)

        def stop() -> None:
            '''Release the active mock search wait; return None.'''
            if self.search_started.is_set():
                self.stop_notified.set()
                self.search_release.set()

        self.engine.reader = Mock()
        self.engine.reader.readline.side_effect = self.commands.get
        self.engine.writer = Mock()
        self.engine.writer.write.side_effect = write
        self.player.stop_evaluation.side_effect = stop
        self.protocol_thread = Thread(target=self.engine.run, daemon=True)
        self.protocol_thread.start()
        self.addCleanup(self._close_protocol)

    def _close_protocol(self) -> None:
        '''Stop the input loop and mocked search reliably.
        Returns:
            None: No return value.
        '''
        self.search_release.set()
        self.commands.put('quit\n')
        self.protocol_thread.join(2.0)
        self.assertFalse(self.protocol_thread.is_alive())

    def _wait_output(self, prefix: str, count: int = 1) -> None:
        '''Wait for count (int) responses starting with prefix (str); return None.'''
        with self.output_condition:
            arrived = self.output_condition.wait_for(
                lambda: sum(line.startswith(prefix) for line in self.output) >= count,
                timeout=2.0)
        self.assertTrue(arrived, self.output)

    def _blocking_evaluation(self, **kwargs: object) -> list[Candidate]:
        '''Accept search arguments (kwargs: object) and return candidates after stopping.'''
        self.search_started.set()
        self.assertTrue(self.search_release.wait(2.0))
        return self.candidates

    def test_run_stop_and_isready(self) -> None:
        '''Check readiness, stop, and repeated stop commands through the actual input loop.
        Returns:
            None: No return value.
        '''
        self._start_protocol()
        self.player.evaluate.side_effect = self._blocking_evaluation
        self.commands.put('go byoyomi 60000\n')
        self.assertTrue(self.search_started.wait(1.0))

        # Answer isready without stopping; release the search only on stop.
        self.commands.put('isready\n')
        self._wait_output('readyok')
        self.assertFalse(self.stop_notified.is_set())
        self.assertNotIn('bestmove 7g7f', self.output)
        self.commands.put('stop\n')
        self._wait_output('bestmove ')
        self.assertTrue(self.stop_notified.is_set())

        # Keep search and final response counts at one after additional stop commands.
        self.commands.put('stop\n')
        self.commands.put('stop\n')
        self.commands.put('isready\n')
        self._wait_output('readyok', 2)
        self.assertEqual(self.output.count('bestmove 7g7f'), 1)
        self.assertEqual(self.player.evaluate.call_count, 1)

    def test_run_quit_discards_result(self) -> None:
        '''Release waits on quit and exit without sending later search results.
        Returns:
            None: No return value.
        '''
        self._start_protocol()
        self.player.evaluate.side_effect = self._blocking_evaluation
        self.commands.put('go byoyomi 60000\n')
        self.assertTrue(self.search_started.wait(1.0))
        self.commands.put('quit\n')
        self.protocol_thread.join(1.0)
        self.assertFalse(self.protocol_thread.is_alive())
        self.assertTrue(self.stop_notified.is_set())
        self.assertFalse(any(line.startswith('bestmove ') for line in self.output))

    def test_run_finished_search_ignores_stop(self) -> None:
        '''Ignore stop and ponderhit after normal completion without additional responses.
        Returns:
            None: No return value.
        '''
        self._start_protocol()
        self.commands.put('go byoyomi 10000\n')
        self._wait_output('bestmove ')
        for command in ('stop', 'stop', 'ponderhit', 'isready'):
            self.commands.put(command + '\n')
        self._wait_output('readyok')
        self.assertEqual(self.output.count('bestmove 7g7f'), 1)
        self.assertEqual(self.player.evaluate.call_count, 1)

    def test_run_ponder_and_infinite_reply_once(self) -> None:
        '''Keep pondering results and return them once on stop or ponderhit.
        Returns:
            None: No return value.
        '''
        self._start_protocol()
        for count, (mode, command) in enumerate(
                (('ponder', 'ponderhit'), ('infinite', 'stop')), 1):
            self.commands.put(f'go {mode}\n')
            self.commands.put('isready\n')
            self._wait_output('readyok', count)
            self.assertEqual(self.output.count('bestmove 7g7f'), count - 1)
            self.commands.put(command + '\n')
            self._wait_output('bestmove ', count)
        self.assertEqual(self.output.count('bestmove 7g7f'), 2)

    def test_run_terminal_ponder(self) -> None:
        '''Retain terminal pondering requests and return their result once on stop.
        Returns:
            None: No return value.
        '''
        self._start_protocol()
        for count, checkmate in enumerate((True, False), 1):
            board = Mock()
            board.get_turn.return_value = 0
            board.get_legal_moves.return_value = [] if checkmate else [Mock()]
            board.is_nyugyoku.return_value = not checkmate
            self.player.get_board.return_value = board
            self.commands.put('go ponder\n')
            self.commands.put('isready\n')
            self._wait_output('readyok', count)
            self.assertEqual(sum(line.startswith('bestmove ') for line in self.output), count - 1)
            self.commands.put('stop\n')
            self._wait_output('bestmove ', count)
            expected = 'bestmove resign' if checkmate else 'bestmove win'
            self.assertEqual(self.output[-1], expected)
        self.player.evaluate.assert_not_called()

    def test_run_position_discards_old_result(self) -> None:
        '''Stop old searches on position changes and keep their results out of the next go.
        Returns:
            None: No return value.
        '''
        self._start_protocol()
        self.player.evaluate.side_effect = self._blocking_evaluation
        self.commands.put('go byoyomi 60000\n')
        self.assertTrue(self.search_started.wait(1.0))
        self.commands.put('position startpos moves 7g7f\n')
        self.commands.put('isready\n')
        self._wait_output('readyok')
        self.assertFalse(any(line.startswith('bestmove ') for line in self.output))
        self.player.play.assert_called_once()
        self.commands.put('go byoyomi 10000\n')
        self._wait_output('bestmove ')
        self.assertEqual(self.output.count('bestmove 7g7f'), 1)

    def test_run_stop_before_search(self) -> None:
        '''Keep early stop requests and return a legal move before search startup.
        Returns:
            None: No return value.
        '''
        self._start_protocol()
        entered = Event()
        original = self.engine._perform

        def perform(command: str, request: _SearchRequest | None = None) -> None:
            '''Delay command (str) and request (_SearchRequest | None) execution; return None.'''
            if request is not None:
                entered.set()
                self.assertTrue(self.search_release.wait(2.0))
            original(command, request)

        # Process stop after registering go but before the actual search begins.
        with patch.object(self.engine, '_perform', side_effect=perform):
            self.commands.put('go byoyomi 60000\n')
            self.assertTrue(entered.wait(1.0))
            self.commands.put('stop\n')
            self.commands.put('isready\n')
            self._wait_output('readyok')
            self.search_release.set()
            self._wait_output('bestmove ')
        legal_move = self.player.get_board.return_value.get_legal_moves()[0]
        self.assertIn(f'bestmove {usi_move_to_string(*legal_move)}', self.output)
        self.player.evaluate.assert_not_called()

    def test_mate_is_not_normal_search(self) -> None:
        '''Do not treat mate-search time as remaining time for normal search.
        Returns:
            None: No return value.
        '''
        # Return the mate-search response without fetching the board or starting search.
        for args in ('mate 60000', 'mate infinite', 'mate'):
            with self.subTest(args=args):
                self.assertEqual(
                    self.engine._perform_command(f'go {args}'),
                    (True, 'checkmate notimplemented', False))
        self.player.get_board.assert_not_called()
        self.player.evaluate.assert_not_called()
