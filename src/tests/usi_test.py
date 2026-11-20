import struct
import unittest
from io import StringIO
from queue import Queue
from threading import Condition, Event, Thread
from unittest.mock import Mock, patch

from deepshogi.board import Board
from deepshogi.config import (COLOR_BLACK, COLOR_NONE, COLOR_WHITE,
                              DEFAULT_INITIAL_SFEN, RESULT_NONE)
from deepshogi.player import Candidate, Player, Referee
from deepshogi.usi import (USIEngine, _SearchRequest, usi_candidate_to_string,
                           usi_move_to_string, usi_string_to_move)
from deepshogi.validation import INT32_MAX


class UsiInitializationTest(unittest.TestCase):
    '''Check lazy creation of the inference processor and state management at isready.'''

    def setUp(self) -> None:
        '''Prepare an engine with model loading and player creation replaced.
        '''
        # Track the number of creations and when readyok is sent.
        self.processor = Mock()
        self.factory = Mock(return_value=self.processor)
        self.writer = StringIO()
        self.engine = USIEngine(
            processor=None, threads=1, processor_factory=self.factory, writer=self.writer)
        player_patch = patch('deepshogi.usi.Player')
        self.player_class = player_patch.start()
        self.player = self.player_class.return_value
        self.addCleanup(player_patch.stop)

    def test_requires_exactly_one_processor_source(self) -> None:
        '''Reject specifying both a processor and a factory, and specifying neither.
        '''
        # Reject invalid combinations before loading the model.
        with self.assertRaises(ValueError):
            USIEngine(processor=None, threads=1)
        with self.assertRaises(ValueError):
            USIEngine(processor=self.processor, threads=1, processor_factory=self.factory)
        self.factory.assert_not_called()

    def test_usi_and_quit_do_not_create_processor(self) -> None:
        '''Do not create the inference processor for usi and quit alone.
        '''
        # Engine information can be returned and the engine can exit without loading the model.
        self.engine.reader = StringIO('usi\nquit\n')
        self.engine.run()
        self.assertIn('usiok', self.writer.getvalue().splitlines())
        self.factory.assert_not_called()
        self.player_class.assert_not_called()

    def test_isready_waits_for_initialization_and_reuses_processor(self) -> None:
        '''Respond after initialization and reuse the processor across settings and games.
        '''
        # Do not send readyok until the player initialization completes.
        def initialize(**kwargs: object) -> None:
            '''Check the response is not sent before the initialization.
            Args:
                kwargs (object): Keyword arguments of the player initialization.
            '''
            self.assertNotIn('readyok', self.writer.getvalue().splitlines())

        self.player.initialize.side_effect = initialize
        self.engine._perform('isready')
        self.assertEqual(self.writer.getvalue(), 'readyok\n')
        self.player_class.assert_called_once()
        self.assertIs(self.player_class.call_args.kwargs['processor'], self.processor)
        self.player.initialize.side_effect = None

        # Do not recreate the player for the same settings, and apply only setting changes.
        self.engine._perform('isready')
        self.player_class.assert_called_once()
        self.engine._perform('setoption name Threads value 2')
        self.engine._perform('isready')
        self.assertEqual(self.player_class.call_count, 2)
        self.assertEqual(self.player_class.call_args.kwargs['threads'], 2)

        # Do not reload the model at isready during or after a game.
        for command in ('usinewgame', 'isready', 'gameover', 'isready'):
            self.engine._perform(command)
        self.factory.assert_called_once_with()
        self.assertEqual(self.player_class.call_count, 2)

    def test_processor_failure_does_not_respond_and_can_retry(self) -> None:
        '''Do not send readyok on creation failure and retry creation at the next isready.
        '''
        # Do not expose the failed result or proceed to create a player.
        self.factory.side_effect = [RuntimeError('model load failed'), self.processor]
        with self.assertLogs('deepshogi.usi', level='ERROR'):
            self.engine._perform('isready')
        self.assertEqual(self.writer.getvalue(), '')
        self.assertIsNone(self.engine.processor)
        self.assertIsNone(self.engine.player)
        self.player_class.assert_not_called()

        # Send readyok only when the retry succeeds.
        self.engine._perform('isready')
        self.assertEqual(self.writer.getvalue(), 'readyok\n')
        self.assertEqual(self.factory.call_count, 2)
        self.assertIs(self.engine.processor, self.processor)

    def test_player_failure_keeps_loaded_processor_for_retry(self) -> None:
        '''Retry with the loaded processor after the player initialization fails.
        '''
        # Handle a successful model creation and the player readiness separately.
        self.player.initialize.side_effect = [RuntimeError('player init failed'), None]
        with self.assertLogs('deepshogi.usi', level='ERROR'):
            self.engine._perform('isready')
        self.assertEqual(self.writer.getvalue(), '')
        self.assertIsNone(self.engine.player)
        self.assertIsNone(self.engine._player_settings)
        self.assertIs(self.engine.processor, self.processor)

        # Recreate only the player and do not send readyok until it succeeds.
        self.engine._perform('isready')
        self.assertEqual(self.writer.getvalue(), 'readyok\n')
        self.factory.assert_called_once_with()
        self.assertEqual(self.player_class.call_count, 2)


class UsiTest(unittest.TestCase):
    '''Verify USI time arguments and search control branches.'''

    def setUp(self) -> None:
        '''Prepare an engine with mocked searches.
        '''
        # Inspect search arguments without loading a model.
        self.engine = USIEngine(
            processor=Mock(), threads=1, timelimit=1000.0, initial_turn=0)
        self.player = Mock()
        self.player.referee.draw_turn = 512
        self.player.referee.judge.return_value = (False, COLOR_NONE, RESULT_NONE)
        self.player.get_visits.return_value = 100
        candidate = Candidate(
            (6, 6), (6, 5), False, COLOR_BLACK, 100, 1.0, 0.0, 1.0, [])
        self.player.evaluate.return_value = [candidate]
        self.player.get_random_candidate.return_value = candidate
        self.candidates = [candidate]
        self.engine.player = self.player
        self.engine._player_settings = self.engine._get_player_settings()
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
        '''
        # Set the actual board move count from SFEN independently of the move history.
        fields = DEFAULT_INITIAL_SFEN.split()
        fields[1] = 'b' if color == COLOR_BLACK else 'w'
        fields[3] = str(current_moves + 1)
        self.player.get_board.return_value = Board(' '.join(fields))
        self.player.get_color.return_value = color

    def test_spin_options_advertise_complete_supported_ranges(self) -> None:
        '''Check that every advertised spin option has bounds supported by native types.
        '''
        # Parse each response line as a GUI would, detecting missing upper bounds.
        _, message, _ = self.engine._perform_command('usi')
        exceptions = {
            'DrawTurn': 32767, 'Timelimit': 2147483500,
            'InitialWidth': 2147483646, 'ResignThreshold': 100,
        }
        spin_names = []
        for line in message.splitlines():
            if not line.startswith('option name ') or ' type spin ' not in line:
                continue

            tokens = line.split()
            name = tokens[2]
            fields = dict(zip(tokens[5::2], tokens[6::2]))
            with self.subTest(option=name):
                self.assertIn('min', fields)
                self.assertIn('max', fields)
                minimum, default, maximum = (int(fields[key]) for key in ('min', 'default', 'max'))
                self.assertLessEqual(minimum, default)
                self.assertLessEqual(default, maximum)
                self.assertEqual(maximum, exceptions.get(name, INT32_MAX))
            spin_names.append(name)
        self.assertTrue(spin_names)

    def test_maximum_random_width_and_timelimit_fit_native_wait(self) -> None:
        '''Check that maximum GUI settings keep random move wait arguments within int32_t.
        '''
        # Exercise the actual Python random selection path up to the native call.
        self.engine.initial_turn = 4
        self.engine._perform_command('setoption name InitialWidth value 2147483646')
        self.engine._perform_command('setoption name Timelimit value 2147483500')
        self.player.native.get_candidates.return_value = [
            ((6, 6), (6, 5), False, COLOR_BLACK, 1, 1.0, 0.0, 1.0, [])]
        player = object.__new__(Player)
        player.native = self.player.native
        player.referee = Referee()
        self.player.get_random_candidate.side_effect = player.get_random_candidate
        _, message, _ = self.engine._perform_command('go')
        self.assertEqual(message.splitlines()[-1], 'bestmove 7g7f')
        visits, seconds, stop = self.player.native.wait_evaluation.call_args.args
        self.assertEqual(visits, INT32_MAX)
        self.assertTrue(stop)

        # Check milliseconds do not overflow after Cython float conversion and C++ multiplication.
        native_seconds = struct.unpack('f', struct.pack('f', seconds))[0]
        native_ms = struct.unpack('f', struct.pack('f', native_seconds * 1000.0))[0]
        self.assertGreaterEqual(native_ms, 0)
        self.assertLessEqual(native_ms, INT32_MAX)
        struct.pack('i', visits)
        struct.pack('i', int(native_ms))

    def test_candidate_info_depth_and_statistics(self) -> None:
        '''Check PV lengths, shared statistics, and omission of empty variations.
        '''
        # Pass statistics different from candidate visits and ensure PV appears last.
        candidate = self.candidates[0]
        moves = ['7g7f', '3c3d', '2g2f']
        for depth in (0, 1, 3):
            with self.subTest(depth=depth):
                candidate.variations = [usi_string_to_move(move) for move in moves[:depth]]
                message = usi_candidate_to_string(candidate, 2, 2500, 75, 30)
                self.assertIn('multipv 2 time 2500 nodes 75 nps 30', message)
                if depth:
                    self.assertTrue(message.endswith(
                        f' depth {depth} pv ' + ' '.join(moves[:depth])), message)
                else:
                    self.assertNotIn(' depth ', message)
                    self.assertNotIn(' pv ', message)

    def test_go_info_uses_visit_delta_and_monotonic_time(self) -> None:
        '''Check shared monotonic statistics exclude visits from a reused tree.
        '''
        # Report all candidates even for MultiPV=1 without using candidate visits as nodes.
        self.player.evaluate.return_value = self.candidates * 2
        self.player.get_visits.side_effect = [100, 160]
        with patch('deepshogi.usi.time.monotonic', side_effect=[10.0, 10.0, 12.5, 13.0]), \
                patch('deepshogi.usi.time.time', side_effect=AssertionError('wall clock')):
            stat, message, cont = self.engine._perform_command('go')
        self.assertTrue(stat)
        self.assertFalse(cont)
        lines = message.splitlines()
        self.assertEqual(sum(line.startswith('info multipv ') for line in lines), 2)
        for line in lines[:-1]:
            self.assertIn('nodes 60 nps 20', line)
            self.assertIn('time 3000', line)
        self.assertEqual(lines[-1], 'bestmove 7g7f')

    def test_info_zero_elapsed_and_no_candidates(self) -> None:
        '''Check safe statistics without a PV before the first result or at zero elapsed time.
        '''
        # Avoid division by zero and do not invent PV or depth for missing candidates.
        request = _SearchRequest(started_at=10.0, initial_visits=100)
        with patch('deepshogi.usi.time.monotonic', return_value=10.0):
            self.assertEqual(self.engine._format_info(request), 'info nodes 0 nps 0 time 0')
        self.player.get_visits.return_value = 110
        with patch('deepshogi.usi.time.monotonic', return_value=11.0):
            self.assertEqual(
                self.engine._format_info(request), 'info nodes 10 nps 10 time 1000')

    def test_go_info_includes_extended_search(self) -> None:
        '''Check final statistics include player search extensions.
        '''
        # Run Python extensions while advancing time and visits only in the native wait mock.
        self._set_position(21)
        sfen = self.player.get_board.return_value.get_sfen()
        self.player.get_board.side_effect = lambda: Board(sfen)
        self.player.referee.judge.return_value = (False, COLOR_NONE, RESULT_NONE)
        self.engine.extends = 2
        self.engine.visits = 100
        self.engine.criterion = 'visits'
        self.player.native.get_candidates.return_value = [
            (src, dst, False, COLOR_BLACK, 50, 0.5, 0.0, 1.0, [])
            for src, dst in (((6, 6), (6, 5)), ((1, 6), (1, 5)))]
        clock = [10.0]

        def wait(visits: int, timelimit: float, stop: bool) -> None:
            '''Advance the clock and visit count for one simulated search.
            Args:
                visits (int): Target node visit count.
                timelimit (float): Time limit in seconds, including extensions.
                stop (bool): True to stop search after waiting.
            '''
            clock[0] += 2.0
            self.player.get_visits.return_value += 20

        self.player.native.wait_evaluation.side_effect = wait
        self.player.evaluate.side_effect = lambda **kwargs: Player.evaluate(self.player, **kwargs)
        with patch('deepshogi.usi.time.monotonic', side_effect=lambda: clock[0]):
            _, message, _ = self.engine._perform_command('go')
        targets = [call.args[0] for call in self.player.native.wait_evaluation.call_args_list]
        self.assertEqual(targets, [100, 150, 200])
        self.assertIn('nodes 60 nps 10 time 6000', message)

    def test_ponder_info_accumulates_and_next_go_resets(self) -> None:
        '''Check ponder updates share measurements and the next go resets the baseline.
        '''
        # Advance time and visits for each result without waiting in real time.
        self.engine.ponder = True
        request = _SearchRequest(mode='ponder')
        clock = [10.0]
        updates = iter(((11.0, 120), (13.0, 160), (22.0, 200)))

        def evaluate(**kwargs: object) -> list[Candidate]:
            '''Accept search arguments, advance simulated statistics, and return candidates.
            Args:
                kwargs (object): Search settings passed to Player.
            Returns:
                list[Candidate]: Updated candidate list.
            '''
            clock[0], self.player.get_visits.return_value = next(updates)
            return self.candidates

        self.player.evaluate.side_effect = evaluate
        with patch('deepshogi.usi.time.monotonic', side_effect=lambda: clock[0]), \
                patch('deepshogi.usi.Thread') as thread, \
                patch.object(request.stop_event, 'wait', return_value=False):
            for expected in ('nodes 20 nps 20 time 1000', 'nodes 60 nps 20 time 3000'):
                self.assertEqual(
                    self.engine._perform_command_go(['ponder'], request=request),
                    (True, '', True))
                self.assertIn(expected, self.engine._format_info(request))
            thread.assert_called_once()

            # Report the accumulated statistics without searching again after a stop.
            request.stop_event.set()
            _, message, cont = self.engine._perform_command_go(['ponder'], request=request)
            self.assertFalse(cont)
            self.assertIn('nodes 60 nps 20 time 3000', message)
            self.assertEqual(self.player.evaluate.call_count, 2)
            self.assertTrue(request.info_finished.is_set())

            # Start the next request's measurements excluding the existing 160 visits.
            clock[0] = 20.0
            _, message, _ = self.engine._perform_command('go')
            self.assertIn('nodes 40 nps 20 time 2000', message)

    def test_analyze_false_does_not_measure_or_report(self) -> None:
        '''Check direct calls return only the move when analysis is disabled.
        '''
        # Do not measure visits or start a reporting thread on the debug path.
        with patch('deepshogi.usi.Thread') as thread:
            self.assertEqual(
                self.engine._perform_command_go([], analyze=False),
                (True, 'bestmove 7g7f', False))
        self.player.get_visits.assert_not_called()
        thread.assert_not_called()

    def test_report_info_interval_and_failure(self) -> None:
        '''Check the one-second reporting interval and termination after output failure.
        '''
        # Simulate reporting termination and check the interval and cached candidates.
        request = _SearchRequest(started_at=0.0, candidates=self.candidates)
        with patch.object(request.info_finished, 'wait', side_effect=[False, True]) as wait:
            self.engine.writer = Mock()
            self.engine._report_info(request)
        self.assertEqual([call.args for call in wait.call_args_list], [(1.0,), (1.0,)])
        self.assertIn('info multipv 1', self.engine.writer.write.call_args.args[0])
        self.player.evaluate.assert_not_called()
        self.player.native.get_candidates.assert_not_called()

        # Log output errors and end reporting without restarting the search.
        self.engine.writer.write.side_effect = OSError('writer closed')
        with patch.object(request.info_finished, 'wait', return_value=False):
            self.engine._report_info(request)
        self.player.evaluate.assert_not_called()

    def test_go_time_arguments(self) -> None:
        '''Check time units, side selection, omitted values, and explicit zeroes.
        '''
        # Check time passed to search in seconds against fixed expected values.
        cases = [
            (COLOR_BLACK, 'btime 600000 wtime 300000', 335.0),
            (COLOR_WHITE, 'btime 600000 wtime 300000', 35.0),
            (COLOR_BLACK, 'btime 40000 wtime 50000 binc 10000 winc 20000', 40.0),
            (COLOR_WHITE, 'btime 40000 wtime 50000 binc 10000 winc 20000', 60.0),
            (COLOR_BLACK, 'btime 60000 wtime 50000 byoyomi 10000', 68.0),
            (COLOR_WHITE, 'btime 60000 wtime 50000 byoyomi 10000', 58.0),
            (COLOR_BLACK, 'byoyomi 10000', 8.0),
            (COLOR_BLACK, 'binc 10000', 0.0),
            (COLOR_WHITE, 'winc 10000', 0.0),
            # Check the two-second byoyomi boundary; increments do not count toward the margin.
            (COLOR_BLACK, 'btime 60000 byoyomi 1999', 51.999),
            (COLOR_BLACK, 'btime 60000 byoyomi 2000', 60.0),
            (COLOR_BLACK, 'btime 60000 byoyomi 2001', 60.001),
            (COLOR_BLACK, 'btime 60000 byoyomi 1000 binc 10000', 61.0),
            (COLOR_BLACK, 'byoyomi 1999', 0.0),
            (COLOR_BLACK, 'byoyomi 2000', 0.0),
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

    def test_extends_by_total_moves(self) -> None:
        '''Check extension counts depend on total moves for normal and ponder searches.
        '''
        # Use the SFEN move count rather than history length and preserve saved settings.
        self.engine.ponder = True
        for configured_extends in (0, 2):
            self.engine.extends = configured_extends
            for mode in ('', 'ponder', 'infinite'):
                for current_moves, expected in (
                    (0, 0), (19, 0), (20, 0), (21, configured_extends), (0, 0),
                ):
                    with self.subTest(
                            extends=configured_extends, mode=mode, current_moves=current_moves), \
                            patch('deepshogi.usi.time.sleep'):
                        self._set_position(current_moves)
                        self.engine._perform_command(f'go {mode}')
                        self.assertEqual(
                            self.player.evaluate.call_args.kwargs['extends'], expected)
                        self.assertEqual(self.engine.extends, configured_extends)

    def test_draw_turn_initialization(self) -> None:
        '''Pass the configured draw limit during preparation, resets, and game completion.
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
        '''
        # Use different engine and player limits to check the active search conditions.
        self.engine.draw_turn = 300
        self.assertEqual(self.engine.moves, [])
        for draw_turn, current_moves, expected in (
            (200, 100, 941.0),
            (201, 100, 940.0),
            (200, 197, 989.0),
            (200, 198, 990.0),
            (200, 199, 990.0),
            (200, 200, 990.0),
            (200, 201, 990.0),
        ):
            with self.subTest(draw_turn=draw_turn, current_moves=current_moves):
                self.player.referee.draw_turn = draw_turn
                self._set_position(current_moves)
                self.engine._perform_command('go btime 1000000 wtime 1000000')
                self.assertAlmostEqual(
                    self.player.evaluate.call_args.kwargs['timelimit'], expected)

    def test_player_settings_apply_on_isready(self) -> None:
        '''Check initial construction settings and recreation after each setting changes.; return
            None.'''
        # Save settings before initial readiness without constructing a player.
        self.engine.player = None
        self.engine._perform_command('setoption name Threads value 2')
        self.engine._perform_command('setoption name MaxVisits value 200')
        with patch('deepshogi.usi.Player', return_value=self.player) as constructor:
            self.engine._perform_command_isready([])
            self.assertEqual(constructor.call_args.kwargs['threads'], 2)
            self.assertEqual(constructor.call_args.kwargs['max_visits'], 200)
            self.engine._perform_command_isready([])
            self.assertEqual(constructor.call_count, 1)

            # Apply each changed construction setting once at the next readiness check.
            changes: list[tuple[str, str, str, object]] = [
                ('Threads', '3', 'threads', 3),
                ('MaxVisits', '300', 'max_visits', 300),
                ('NyugyokuRule', '24', 'nyugyoku_scores', (31, 31)),
                ('CheckSearchDepth', '3', 'check_search_depth', 3),
                ('CheckSearchNode', '100', 'check_search_node', 100),
                ('CheckNodeDepth', '0', 'check_node_depth', 0),
                ('PucbConstantInit', '250', 'pucb_constant_init', 2.5),
                ('PucbConstantBase', '100', 'pucb_constant_base', 100.0),
                ('PucbMinVisitsRate', '50', 'pucb_min_visits_rate', 0.5),
            ]
            for count, (name, value, argument, expected) in enumerate(changes, 1):
                with self.subTest(option=name):
                    self.engine._perform_command(f'setoption name {name} value {value}')
                    self.assertEqual(constructor.call_count, count)
                    self.engine._perform_command_isready([])
                    self.assertEqual(constructor.call_count, count + 1)
                    self.assertEqual(constructor.call_args.kwargs[argument], expected)
                    self.engine._perform_command_isready([])
                    self.assertEqual(constructor.call_count, count + 1)

    def test_pucb_base_option_validation_and_application(self) -> None:
        '''Check PUCB base bounds, preservation after invalid input, and valid updates.; return
            None.'''
        # Match advertised GUI bounds to the accepted input range.
        _, message, _ = self.engine._perform_command('usi')
        self.assertIn('option name PucbConstantBase type spin default 3200 min 1', message)
        previous = self.engine.pucb_constant_base
        applied = self.engine._player_settings
        self.engine._game_in_progress = False
        with patch('deepshogi.usi.Player') as constructor:
            for value in ('0', '-0', '-1', '0.5', '1.5', '1.0', 'nan', 'inf', '-inf',
                          '1e100', str(10 ** 100), 'abc'):
                with self.subTest(value=value):
                    _, error, _ = self.engine._perform_command(
                        f'setoption name PucbConstantBase value {value}')
                    self.assertTrue(error)
                    self.assertEqual(self.engine.pucb_constant_base, previous)
                    self.assertEqual(self.engine._player_settings, applied)
                    self.engine._perform_command_isready([])
                    constructor.assert_not_called()

            # Recreate once after valid input and pass validated settings.
            result = self.engine._perform_command('setoption name PucbConstantBase value 1')
            self.assertEqual(result, (False, '', False))
            self.engine._perform_command_isready([])
            constructor.assert_called_once()
            self.assertEqual(constructor.call_args.kwargs['pucb_constant_base'], 1.0)
            self.engine._perform_command_isready([])
            constructor.assert_called_once()

    def test_initial_value_delta_option_validation(self) -> None:
        '''Apply the win probability tolerance and keep the setting on invalid values.
        '''
        # Divide the option value by 100 and set zero and a positive value.
        for value, expected in (('0', 0.0), ('5', 0.05)):
            with self.subTest(value=value):
                result = self.engine._perform_command(
                    f'setoption name InitialValueDelta value {value}')
                self.assertEqual(result, (False, '', False))
                self.assertEqual(self.engine.initial_value_delta, expected)

        # Return an error and keep the last accepted value.
        for value in ('-1', 'nan', 'inf', '-inf'):
            with self.subTest(value=value):
                _, error, _ = self.engine._perform_command(
                    f'setoption name InitialValueDelta value {value}')
                self.assertIn('InitialValueDelta', error)
                self.assertEqual(self.engine.initial_value_delta, 0.05)

    def test_usi_ponder_alias_applies_to_search(self) -> None:
        '''Check the latest ponder alias controls predicted replies without continuing normal
            search.'''
        # Check both aliases enable and disable the same saved setting.
        self.candidates[0].variations = [
            usi_string_to_move(move) for move in ('7g7f', '3c3d')]
        changes = (
            ('USI_Ponder', True), ('USI_Ponder', False), ('Ponder', True),
            ('USI_Ponder', False), ('USI_Ponder', True), ('Ponder', False),
        )
        for random_move in (False, True):
            self.engine.initial_turn = 4 if random_move else 0
            for name, enabled in changes:
                with self.subTest(random_move=random_move, name=name, enabled=enabled):
                    self.player.reset_mock()
                    result = self.engine._perform_command(
                        f'setoption name {name} value {str(enabled).lower()}')
                    self.assertEqual(result, (False, '', False))
                    self.assertEqual(self.engine.ponder, enabled)
                    _, message, _ = self.engine._perform_command('go byoyomi 10000')
                    search = (self.player.get_random_candidate if random_move
                              else self.player.evaluate)
                    self.assertFalse(search.call_args.kwargs['ponder'])
                    self.assertEqual(' ponder 3c3d' in message, enabled)
                    self.player.stop_evaluation.assert_not_called()

    def test_bestmove_requires_consistent_legal_pv(self) -> None:
        '''Reject predicted replies for short PVs, inconsistent first moves, or illegal replies.
        '''
        self.engine.ponder = True
        board = self.player.get_board.return_value
        original_sfen = board.get_sfen()
        cases = (
            ((), 'bestmove 7g7f'),
            (('7g7f',), 'bestmove 7g7f'),
            (('2g2f', '3c3d'), 'bestmove 7g7f'),
            (('7g7f', '7f7e'), 'bestmove 7g7f'),
            (('7g7f', '3c3d'), 'bestmove 7g7f ponder 3c3d'),
        )
        for pv, expected in cases:
            with self.subTest(pv=pv):
                self.candidates[0].variations = [usi_string_to_move(move) for move in pv]
                _, message, _ = self.engine._perform_command('go')
                self.assertEqual(message.splitlines()[-1], expected)
                self.assertEqual(board.get_sfen(), original_sfen)
                self.assertEqual(self.engine.moves, [])
                self.player.play.assert_not_called()
                self.player.referee.update.assert_not_called()

    def test_bestmove_omits_ponder_after_game_end(self) -> None:
        '''Omit predicted replies after terminal moves, resignation, or entering-king declarations.
        '''
        self.engine.ponder = True
        self.candidates[0].variations = [
            usi_string_to_move(move) for move in ('7g7f', '3c3d')]

        # Do not ponder after a terminal move even if the PV is legal.
        self.player.referee.judge.return_value = (True, COLOR_BLACK, RESULT_NONE)
        _, message, _ = self.engine._perform_command('go')
        self.assertEqual(message.splitlines()[-1], 'bestmove 7g7f')

        # Do not request pondering from a candidate PV when resigning on win probability.
        self.engine.moves = ['7g7f']
        self.engine.resign_threshold = 1.0
        _, message, _ = self.engine._perform_command('go')
        self.assertEqual(message.splitlines()[-1], 'bestmove resign')

        # Return only terminal replies for initial checkmate or entering-king declarations.
        board = Mock()
        self.player.get_board.return_value = board
        for legal_moves, nyugyoku, expected in (
            ([], False, 'bestmove resign'), ([Mock()], True, 'bestmove win'),
        ):
            board.get_legal_moves.return_value = legal_moves
            board.is_nyugyoku.return_value = nyugyoku
            self.assertEqual(self.engine._perform_command('go'), (True, expected, False))

    def test_random_bestmove_uses_selected_candidates_pv(self) -> None:
        '''Use the randomly selected candidate's own PV for the predicted reply.
        '''
        # Use a move and reply different from the normal search's top candidate.
        self.engine.ponder = True
        self.engine.initial_turn = 4
        self.candidates[0].variations = [
            usi_string_to_move(move) for move in ('7g7f', '3c3d')]
        candidate = Candidate((1, 6), (1, 5), False, COLOR_BLACK, 50, 0.5, 0.0, 1.0, [])
        candidate.variations = [usi_string_to_move(move) for move in ('2g2f', '8c8d')]
        self.player.get_random_candidate.return_value = candidate
        _, message, _ = self.engine._perform_command('go')
        self.assertEqual(message.splitlines()[-1], 'bestmove 2g2f ponder 8c8d')
        self.assertFalse(self.player.get_random_candidate.call_args.kwargs['ponder'])
        self.player.evaluate.assert_not_called()
        self.player.play.assert_not_called()

    def test_random_move_without_candidate_uses_regular_search(self) -> None:
        '''Run a normal search with the remaining time if no nonrepeating random candidate exists.
        '''
        self.engine.initial_turn = 4
        now = 10.0

        def exhaust_random_candidates(**kwargs: object) -> None:
            '''Use two seconds for the random move and return no candidate.
            Args:
                kwargs (object): Keyword arguments of the random move.
            '''
            nonlocal now
            now += 2.0

        self.player.get_random_candidate.side_effect = exhaust_random_candidates
        with patch('deepshogi.usi.time.monotonic', side_effect=lambda: now):
            _, message, _ = self.engine._perform_command('go byoyomi 10000')
        self.assertEqual(message.splitlines()[-1], 'bestmove 7g7f')
        self.assertEqual(self.player.get_random_candidate.call_args.kwargs['timelimit'], 8.0)
        self.assertEqual(self.player.evaluate.call_args.kwargs['timelimit'], 6.0)

    def test_bestmove_formats_promotion_and_drop(self) -> None:
        '''Format legal PVs containing promotions and drops with predicted replies.
        '''
        # Use a position allowing an opponent pawn drop after the selected promotion.
        self.engine.ponder = True
        self.player.get_board.return_value = Board(
            '4k4/9/2P6/9/9/9/9/9/4K4 b p 1')
        candidate = Candidate((6, 2), (6, 1), True, COLOR_BLACK, 100, 1.0, 0.0, 1.0, [])
        candidate.variations = [usi_string_to_move(move) for move in ('7c7b+', 'P*4e')]
        self.player.evaluate.return_value = [candidate]
        _, message, _ = self.engine._perform_command('go')
        self.assertEqual(message.splitlines()[-1], 'bestmove 7c7b+ ponder P*4e')

    def test_stop_takes_priority_over_ponderhit_at_send(self) -> None:
        '''Check late stop and repeated control commands cannot restore a ponder request.
        '''
        self.engine.writer = Mock()
        for commands in (('ponderhit', 'stop'), ('stop', 'ponderhit')):
            with self.subTest(commands=commands):
                request = _SearchRequest(mode='ponder')
                self.engine._request = request
                for command in commands:
                    self.engine._perform_command(command)
                self.assertEqual(request.stop_reason, 'stop')
                self.assertTrue(request.stop_event.is_set())

                # Remove a predicted reply at send time even if it was formatted before stop.
                self.engine._write_response('info nodes 10\nbestmove 7g7f ponder 3c3d', request)
                self.assertEqual(
                    self.engine.writer.write.call_args.args[0], 'info nodes 10\nbestmove 7g7f\n')
                writes = self.engine.writer.write.call_count
                self.engine._perform_command('ponderhit')
                self.engine._write_response('bestmove 7g7f ponder 3c3d', request)
                self.assertEqual(self.engine.writer.write.call_count, writes)

    def test_ponderhit_does_not_stop_normal_or_infinite(self) -> None:
        '''Ignore ponderhit during normal and infinite searches.
        '''
        for mode in ('normal', 'infinite'):
            request = _SearchRequest(mode=mode)
            self.engine._request = request
            self.engine._perform_command('ponderhit')
            self.assertIsNone(request.stop_reason)
            self.assertFalse(request.stop_event.is_set())
        self.player.stop_evaluation.assert_not_called()

    def test_disabled_ponder_returns_cached_or_legal_move_without_search(self) -> None:
        '''Return only a cached result or legal move after control input if pondering is disabled.
        '''
        candidate_sets: tuple[list[Candidate], ...] = ([], self.candidates)
        for candidates in candidate_sets:
            for reason in ('stop', 'ponderhit'):
                with self.subTest(candidates=bool(candidates), reason=reason):
                    request = _SearchRequest(
                        mode='ponder', stop_reason=reason, candidates=candidates)
                    request.stop_event.set()
                    _, message, cont = self.engine._perform_command_go(['ponder'], request=request)
                    move = ('7g7f' if candidates else usi_move_to_string(
                        *self.player.get_board.return_value.get_legal_moves()[0]))
                    self.assertEqual(message, f'bestmove {move}')
                    self.assertFalse(cont)
                    self.assertIsNone(request.info_thread)
        self.player.evaluate.assert_not_called()
        self.player.get_random_candidate.assert_not_called()
        self.player.get_visits.assert_not_called()

    def test_usi_hash_keeps_settings_and_player(self) -> None:
        '''Ignore hash settings while preserving saved settings and the player.'''
        # Accept without numeric conversion and do not recreate the player on readiness.
        self.engine.ponder = True
        self.engine._game_in_progress = False
        previous = vars(self.engine).copy()
        previous_response = self.engine._perform_command('usi')
        with patch('deepshogi.usi.Player') as constructor:
            for value in ('1', '1024', '0', '-1', 'abc'):
                with self.subTest(value=value):
                    result = self.engine._perform_command(
                        f'setoption name USI_Hash value {value}')
                    self.assertEqual(result, (False, '', False))
                    self.assertEqual(vars(self.engine), previous)
                    self.assertEqual(self.engine._perform_command('usi'), previous_response)
                    self.assertEqual(
                        self.engine._perform_command('isready'), (True, 'readyok', False))
                    constructor.assert_not_called()
        self.player.initialize.assert_not_called()
        self.player.stop_evaluation.assert_not_called()

    def test_standard_options_preserve_syntax_and_advertisement(self) -> None:
        '''Reject missing standard option arguments and preserve advertised options.'''
        # Report missing values before resolving aliases or ignoring options.
        self.engine.ponder = True
        previous = vars(self.engine).copy()
        for name in ('USI_Ponder', 'USI_Hash'):
            for suffix in ('', ' value'):
                with self.subTest(name=name, suffix=suffix):
                    result = self.engine._perform_command(f'setoption name {name}{suffix}')
                    self.assertEqual(result, (False, 'syntax error', False))
                    self.assertEqual(vars(self.engine), previous)

        # Advertise only existing option names and keep rejecting unknown options.
        _, message, _ = self.engine._perform_command('usi')
        self.assertIn('option name Ponder type check default true', message)
        self.assertNotIn('option name USI_Ponder ', message)
        self.assertNotIn('option name USI_Hash ', message)
        self.assertEqual(
            self.engine._perform_command('setoption name UnknownOption value 1'),
            (False, 'unknown option: UnknownOption', False))

    def test_reverted_player_settings_keep_player(self) -> None:
        '''Check reverted construction settings do not recreate the player; return None.'''
        # Keep the player if current values match, despite temporary changes.
        self.engine._perform_command('setoption name Threads value 2')
        self.engine._perform_command('setoption name Threads value 1')
        with patch('deepshogi.usi.Player') as constructor:
            self.engine._perform_command_isready([])
            constructor.assert_not_called()
        self.player.initialize.assert_not_called()

    def test_game_defers_player_settings(self) -> None:
        '''Defer construction settings during a game until readiness after game end.; return
            None.'''
        # Keep player configuration after new game even before searching.
        self.engine.ponder = True
        self.engine._perform_command_usinewgame([])
        applied_max_visits = self.engine.max_visits
        self.engine._perform_command('setoption name Threads value 2')
        self.engine._perform_command('setoption name MaxVisits value 200')
        with patch('deepshogi.usi.Player') as constructor:
            self.engine._perform_command_isready([])
            constructor.assert_not_called()
        with patch('deepshogi.usi.time.sleep'):
            self.engine._perform_command('go ponder')
        self.assertEqual(
            self.player.evaluate.call_args.kwargs['visits'], applied_max_visits)

        # Apply the latest configuration after game end and use the new limit next game.
        self.engine._perform_command_gameover([])
        replacement = Mock()
        with patch('deepshogi.usi.Player', return_value=replacement) as constructor:
            self.engine._perform_command_isready([])
            self.assertEqual(constructor.call_args.kwargs['threads'], 2)
            self.assertEqual(constructor.call_args.kwargs['max_visits'], 200)
        self.assertIs(self.engine.player, replacement)
        self.assertEqual(self.engine._get_search_settings().max_visits, 200)

    def test_position_without_newgame_prevents_recreation(self) -> None:
        '''Prevent recreation after a position command even without a new-game notification.; return
            None.'''
        # Preserve board and move history on readiness after a position command.
        self.engine._perform_command('position startpos moves 7g7f')
        self.engine._perform_command('setoption name Threads value 2')
        with patch('deepshogi.usi.Player') as constructor:
            self.engine._perform_command_isready([])
            constructor.assert_not_called()
        self.assertEqual(self.engine.moves, ['7g7f'])

    def test_player_recreation_failure_preserves_state(self) -> None:
        '''Preserve the previous player and active settings after construction or initialization
            fails; return None.'''
        # Do not send readyok after failure; apply the same changes on retry.
        for fail_initialization in (False, True):
            with self.subTest(initialization=fail_initialization):
                self.engine.threads = 1
                self.engine._player_settings = self.engine._get_player_settings()
                previous_settings = self.engine._player_settings
                self.engine.player = self.player
                self.engine.startsfen = 'previous'
                self.engine.moves = ['7g7f']
                self.engine.writer = Mock()
                self.engine._perform_command('setoption name Threads value 2')
                replacement = Mock()
                if fail_initialization:
                    replacement.initialize.side_effect = RuntimeError('initialization failed')
                    failure: object = replacement
                else:
                    failure = RuntimeError('creation failed')
                with patch('deepshogi.usi.Player', side_effect=[failure, Mock()]):
                    self.engine._perform('isready')
                    self.engine.writer.write.assert_not_called()
                    self.assertIs(self.engine.player, self.player)
                    self.assertEqual(self.engine._player_settings, previous_settings)
                    self.assertEqual(self.engine.startsfen, 'previous')
                    self.assertEqual(self.engine.moves, ['7g7f'])
                    self.engine._perform('isready')
                self.engine.writer.write.assert_called_once_with('readyok\n')
                self.assertIsNot(self.engine.player, self.player)
                self.assertEqual(self.engine.startsfen, DEFAULT_INITIAL_SFEN)
                self.assertEqual(self.engine.moves, [])
                self.assertEqual(self.engine._get_search_settings().max_visits,
                                 previous_settings.max_visits)

    def test_draw_turn_waits_for_initialization(self) -> None:
        '''Apply DrawTurn on initialization rather than readiness or additional moves.; return
            None.'''
        # Do not apply settings on isready if they do not require recreating the player.
        self.engine._perform_command('setoption name DrawTurn value 300')
        with patch('deepshogi.usi.Player') as constructor:
            self.engine._perform_command_isready([])
            constructor.assert_not_called()
        self.engine._perform_command('position startpos moves 7g7f')
        self.player.initialize.assert_not_called()
        self.assertEqual(self.player.referee.draw_turn, 512)

        # Pass the latest saved value when initializing after a rewind.
        self.engine._perform_command('position startpos')
        self.player.initialize.assert_called_once_with(DEFAULT_INITIAL_SFEN, draw_turn=300)

    def test_timelimit_option(self) -> None:
        '''Apply the configured USI time cap, including an explicit zero-second cap.
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
        '''
        # Honor byoyomi-only arguments when selecting random opening moves.
        self.engine.initial_turn = 4
        self.engine._perform_command('go byoyomi 10000')
        self.assertEqual(
            self.player.get_random_candidate.call_args.kwargs['timelimit'], 8.0)
        self.player.evaluate.assert_not_called()

    def test_unknown_arguments(self) -> None:
        '''Skip unknown arguments and parse subsequent time fields.
        '''
        # Handle uppercase commands and time fields surrounding unsupported options.
        self.engine._perform_command(
            'GO unknown depth 5 BTIME 600000 movestogo 10 byoyomi 0 trailing')
        self.assertEqual(self.player.evaluate.call_args.kwargs['timelimit'], 335.0)

    def test_ponder_and_infinite(self) -> None:
        '''Check ponder and infinite update intervals and replies after stopping.
        '''
        # Defer bestmove until control input without calculating clock-based limits.
        self.engine.ponder = True
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

    def _track_protocol_position(self) -> None:
        '''Track the mock player's position using a real board.
        '''
        # Give search a board copy and apply only GUI moves to the current position.
        position = [self.player.get_board.return_value]

        def get_board() -> Board:
            '''Return a copy of the current position.
            Returns:
                Board: Copy of the board tracked by the test.
            '''
            board = Board()
            board.copy_from(position[0])
            return board

        def play(src: tuple[int, int], dst: tuple[int, int], promote: bool) -> None:
            '''Apply the GUI's move to the real board.
            Args:
                src (tuple[int, int]): Source coordinates.
                dst (tuple[int, int]): Destination coordinates.
                promote (bool): True to promote.
            '''
            position[0].play(src, dst, promote)

        def initialize(sfen: str = DEFAULT_INITIAL_SFEN, draw_turn: int = 512) -> None:
            '''Apply position initialization after a ponder miss or other reset.
            Args:
                sfen (str): Initial position in SFEN format.
                draw_turn (int): Move count at which the game is drawn.
            '''
            position[0] = Board(sfen, draw_turn=draw_turn)

        self.player.get_board.side_effect = get_board
        self.player.get_color.side_effect = lambda: position[0].get_color()
        self.player.play.side_effect = play
        self.player.initialize.side_effect = initialize

    def _set_protocol_pv(self, moves: list[str]) -> None:
        '''Set the candidate and PV for the next simulated search.
        Args:
            moves (list[str]): USI principal variation including the candidate itself.
        '''
        # Select a move and predicted reply appropriate to each position.
        variations = [usi_string_to_move(move) for move in moves]
        src, dst, promote = variations[0]
        candidate = Candidate(src, dst, promote, COLOR_BLACK, 100, 1.0, 0.0, 1.0, [])
        candidate.variations = variations
        self.player.evaluate.return_value = [candidate]

    def _start_protocol(self) -> None:
        '''Start the actual USI input loop with a queue as input.
        '''
        # Wait for responses using a condition variable instead of fixed sleeps.
        self.commands: Queue[str] = Queue()
        self.output: list[str] = []
        self.output_condition = Condition()
        self.search_started = Event()
        self.search_release = Event()
        self.stop_notified = Event()

        def write(message: str) -> int:
            '''Record message (str) and return its character count (int).
            Returns:
                int: Number of characters in the recorded message.
            '''
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
        '''Accept search arguments (kwargs: object) and return candidates after a stop.
        Returns:
            list[Candidate]: Candidates returned after the stop notification.
        '''
        self.search_started.set()
        self.assertTrue(self.search_release.wait(2.0))
        return self.candidates

    def test_run_standard_ponder_hit_and_next_request(self) -> None:
        '''Check standard pondering, ponderhit, and communication through the next ponder request.
        '''
        self.engine.ponder = True
        self._track_protocol_position()
        with patch('deepshogi.usi.USI_INFO_INTERVAL', 0.01):
            self._start_protocol()
            self._set_protocol_pv(['7g7f', '3c3d'])
            self.commands.put('go\n')
            self._wait_output('bestmove ')
            self.assertEqual(self.output[-1], 'bestmove 7g7f ponder 3c3d')
            self.assertFalse(self.player.evaluate.call_args.kwargs['ponder'])
            self.assertEqual(self.engine.moves, [])
            self.player.play.assert_not_called()

            # Ponder after the GUI sends the predicted position and include another reply on
            # ponderhit.
            for count, moves, pv in (
                (2, ['7g7f', '3c3d'], ['2g2f', '8c8d']),
                (3, ['7g7f', '3c3d', '2g2f', '8c8d'], ['2f2e', '8d8e']),
            ):
                self._set_protocol_pv(pv)
                updates = sum(line.startswith('info multipv ') for line in self.output)
                self.commands.put('position startpos moves ' + ' '.join(moves) + '\n')
                self.commands.put('go ponder\n')
                self._wait_output('info multipv ', updates + 1)
                self.assertEqual(
                    sum(line.startswith('bestmove ') for line in self.output), count - 1)
                self.assertTrue(self.player.evaluate.call_args.kwargs['ponder'])
                self.assertEqual(self.engine.moves, moves)
                request = self.engine._request
                assert request is not None
                info_thread = request.info_thread
                assert info_thread is not None
                self.commands.put('ponderhit\n')
                self._wait_output('bestmove ', count)
                self.assertEqual(self.output[-1], f'bestmove {pv[0]} ponder {pv[1]}')
                self.assertEqual(request.stop_reason, 'ponderhit')
                self.assertFalse(info_thread.is_alive())
                self.assertIsNone(request.info_thread)
                self.assertTrue(self.output[-2].startswith('info nodes '))

            # Keep tree-reusing position updates without applying predicted moves early.
            self.assertEqual(self.player.play.call_count, 4)
            self.player.initialize.assert_not_called()
            expected = Board()
            for move in ('7g7f', '3c3d', '2g2f', '8c8d'):
                expected.play(*usi_string_to_move(move))
            self.assertEqual(self.player.get_board().get_sfen(), expected.get_sfen())

    def test_run_ponder_miss_returns_to_actual_position(self) -> None:
        '''After a ponder miss, search the actual position and continue to the next ponder request.
        '''
        self.engine.ponder = True
        self._track_protocol_position()
        with patch('deepshogi.usi.USI_INFO_INTERVAL', 0.01):
            self._start_protocol()
            self._set_protocol_pv(['7g7f', '3c3d'])
            self.commands.put('go\n')
            self._wait_output('bestmove ')
            self._set_protocol_pv(['2g2f', '8c8d'])
            updates = sum(line.startswith('info multipv ') for line in self.output)
            self.commands.put('position startpos moves 7g7f 3c3d\n')
            self.commands.put('go ponder\n')
            self._wait_output('info multipv ', updates + 1)
            request = self.engine._request
            assert request is not None
            self.commands.put('stop\n')
            self._wait_output('bestmove ', 2)
            self.assertEqual(self.output[-1], 'bestmove 2g2f')
            self.assertEqual(request.stop_reason, 'stop')
            self.assertIsNone(request.info_thread)

            # Discard the missed prediction and search the actual reply sent by the GUI.
            self.commands.put('position startpos moves 7g7f 4c4d\n')
            self.commands.put('go\n')
            self._wait_output('bestmove ', 3)
            self.assertEqual(self.output[-1], 'bestmove 2g2f ponder 8c8d')
            self.assertFalse(self.player.evaluate.call_args.kwargs['ponder'])
            self.assertEqual(self.engine.moves, ['7g7f', '4c4d'])
            self.player.initialize.assert_called_once_with(DEFAULT_INITIAL_SFEN, draw_turn=512)
            expected = Board()
            for move in ('7g7f', '4c4d'):
                expected.play(*usi_string_to_move(move))
            self.assertEqual(self.player.get_board().get_sfen(), expected.get_sfen())

    def test_run_ponderhit_before_first_result_returns_legal_move(self) -> None:
        '''Return one legal move without extra search if ponderhit precedes the first result.
        '''
        self.engine.ponder = True
        self._start_protocol()
        self.candidates = []
        self.player.evaluate.side_effect = self._blocking_evaluation
        self.commands.put('go ponder\n')
        self.assertTrue(self.search_started.wait(1.0))
        self.commands.put('ponderhit\n')
        self._wait_output('bestmove ')
        move = usi_move_to_string(*self.player.get_board.return_value.get_legal_moves()[0])
        self.assertEqual(self.output[-1], f'bestmove {move}')
        self.assertEqual(self.player.evaluate.call_count, 1)
        self.player.get_random_candidate.assert_not_called()
        request = self.engine._request
        assert request is not None
        self.assertEqual(request.stop_reason, 'ponderhit')
        self.assertIsNone(request.info_thread)

    def test_run_normal_search_reports_before_completion(self) -> None:
        '''Check reporting during normal search and the ordering of final PV, move, and reporting
            end.
        '''
        # Shorten only the reporting interval and control search completion with events.
        with patch('deepshogi.usi.USI_INFO_INTERVAL', 0.01):
            self._start_protocol()
            self.player.evaluate.side_effect = self._blocking_evaluation
            self.candidates[0].variations = [usi_string_to_move('7g7f')]
            self.commands.put('go byoyomi 60000\n')
            self.assertTrue(self.search_started.wait(1.0))
            self._wait_output('info nodes')

            # Advance visits while search continues and check the next report's delta.
            with self.engine._response_lock:
                request = self.engine._request
                assert request is not None
                info_thread = request.info_thread
                assert info_thread is not None
                self.player.get_visits.return_value = 150
                updates = sum(line.startswith('info nodes') for line in self.output)
            self._wait_output('info nodes', updates + 1)
            stats = [line for line in self.output if line.startswith('info nodes')]
            self.assertIn('nodes 50 ', stats[-1])
            self.assertFalse(any(line.startswith('info multipv') for line in self.output))
            self.assertFalse(any(line.startswith('bestmove') for line in self.output))
            self.assertEqual(self.player.evaluate.call_count, 1)
            self.player.native.get_candidates.assert_not_called()

            # Report the latest PV only after evaluation and send no info after bestmove.
            self.search_release.set()
            self._wait_output('bestmove ')
            self.assertFalse(info_thread.is_alive())
            self.assertTrue(request.info_finished.is_set())
            self.assertIsNone(request.info_thread)
            final_index = self.output.index('bestmove 7g7f')
            self.assertIn('depth 1 pv 7g7f', self.output[final_index - 2])
            self.assertTrue(self.output[final_index - 1].startswith('info nodes 50 '))
            self.commands.put('isready\n')
            self._wait_output('readyok')
            self.assertEqual(self.output[final_index + 1:], ['readyok'])

    def test_run_search_failure_finishes_info_thread(self) -> None:
        '''Join periodic reporting after search exceptions without leaving stale information.
        '''
        self._start_protocol()

        def evaluate(**kwargs: object) -> list[Candidate]:
            '''Signal search start and raise a simulated exception after release.
            Args:
                kwargs (object): Search settings passed to Player.
            Raises:
                RuntimeError: Simulated search failure after the wait is released.
            '''
            self.search_started.set()
            self.assertTrue(self.search_release.wait(2.0))
            raise RuntimeError('evaluation failed')

        self.player.evaluate.side_effect = evaluate
        self.commands.put('go\n')
        self.assertTrue(self.search_started.wait(1.0))
        request = self.engine._request
        assert request is not None
        info_thread = request.info_thread
        assert info_thread is not None
        search_thread = self.engine.thread
        assert search_thread is not None
        self.search_release.set()
        search_thread.join(2.0)
        self.assertFalse(search_thread.is_alive())
        self.assertFalse(info_thread.is_alive())
        self.assertIsNone(request.info_thread)
        self.commands.put('isready\n')
        self._wait_output('readyok')
        self.assertEqual(self.output, ['readyok'])

    def _assert_protocol_discards_info(self, command: str) -> None:
        '''Check discarding a request ends active reporting.
        Args:
            command (str): USI command.
        '''
        # Wait for active progress reporting before sending a position change or exit.
        with patch('deepshogi.usi.USI_INFO_INTERVAL', 0.01):
            self._start_protocol()
            self.player.evaluate.side_effect = self._blocking_evaluation
            self.commands.put('go\n')
            self.assertTrue(self.search_started.wait(1.0))
            self._wait_output('info nodes')
            request = self.engine._request
            assert request is not None
            info_thread = request.info_thread
            assert info_thread is not None
            self.commands.put(command + '\n' if command else '')
            if command in ('quit', ''):
                self.protocol_thread.join(2.0)
                self.assertFalse(self.protocol_thread.is_alive())
            else:
                self.commands.put('isready\n')
                self._wait_output('readyok')
                self.assertEqual(self.output[-1], 'readyok')
            self.assertTrue(request.discard)
            self.assertTrue(request.info_finished.is_set())
            self.assertFalse(info_thread.is_alive())
            self.assertIsNone(request.info_thread)
            self.assertFalse(any(line.startswith('bestmove') for line in self.output))
            self.assertFalse(any(line.startswith('info multipv') for line in self.output))

    def test_run_position_finishes_info_thread(self) -> None:
        '''Join old reporting and suppress the final response on a position change.
        '''
        self._assert_protocol_discards_info('position startpos moves 7g7f')

    def test_run_quit_finishes_info_thread(self) -> None:
        '''Join search and reporting on quit and suppress the final response.
        '''
        self._assert_protocol_discards_info('quit')

    def test_run_eof_finishes_info_thread(self) -> None:
        '''Join search and reporting on EOF and suppress the final response.
        '''
        self._assert_protocol_discards_info('')

    def test_run_stop_and_isready(self) -> None:
        '''Check readiness, stop, and repeated stop commands through the actual input loop.
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

    def test_run_setoption_preserves_search_and_final_response(self) -> None:
        '''Save settings during search without stopping; use them on the next go; return None.'''
        # Use a position beyond move twenty to exercise extension setting changes.
        self._set_position(21)
        self.candidates[0].variations = [
            usi_string_to_move(move) for move in ('7g7f', '3c3d')]
        # Use old settings for the active go, including the final resignation check.
        self.engine.moves = ['7g7f']
        self.engine.resign_turn = 3
        self._start_protocol()
        self.player.evaluate.side_effect = self._blocking_evaluation
        self.commands.put('go byoyomi 60000\n')
        self.assertTrue(self.search_started.wait(1.0))
        changes = (
            'Visits value 7', 'Extends value 2', 'Timelimit value 1000',
            'Criterion value visits', 'Ponder value false', 'USI_Ponder value true',
            'USI_Hash value 1024', 'MultiPV value 3',
            'ResignThreshold value 100', 'ResignTurn value 0', 'InitialTurn value 4',
            'InitialWidth value 8', 'InitialTemperature value 50',
            'InitialValueDelta value 20', 'InitialWhiteOnly value true',
            'MaxVisits value 200', 'Threads value 2',
        )
        for change in changes:
            self.commands.put(f'setoption name {change}\n')
        self.commands.put('isready\n')
        self._wait_output('readyok')
        self.assertFalse(self.stop_notified.is_set())
        self.assertFalse(any(line.startswith('bestmove ') for line in self.output))
        self.assertEqual(self.player.evaluate.call_count, 1)
        self.assertEqual(self.player.evaluate.call_args.kwargs['timelimit'], 58.0)
        self.assertFalse(self.player.evaluate.call_args.kwargs['ponder'])
        self.assertTrue(self.engine.ponder)
        self.search_release.set()
        self._wait_output('bestmove ')
        self.assertEqual(self.output.count('bestmove 7g7f'), 1)

        # Change search and resignation settings on the next go without suppressing the previous
        # result.
        self.commands.put('go byoyomi 60000\n')
        self._wait_output('bestmove ', 2)
        self.assertEqual(self.player.evaluate.call_count, 2)
        arguments = self.player.evaluate.call_args.kwargs
        self.assertEqual(arguments['visits'], 7)
        self.assertEqual(arguments['extends'], 2)
        self.assertEqual(arguments['timelimit'], 1.0)
        self.assertEqual(arguments['criterion'], 'visits')
        self.assertEqual(arguments['candidate_width'], 3)
        self.assertTrue(arguments['equally'])
        self.assertFalse(arguments['ponder'])
        self.assertEqual(self.output[-1], 'bestmove resign')
        self.player.get_random_candidate.assert_not_called()

    def test_run_settings_fixed_before_search_thread_starts(self) -> None:
        '''Preserve settings from go receipt even if its search thread is delayed; return None.'''
        self._start_protocol()
        entered = Event()
        original = self.engine._perform

        def perform(command: str, request: _SearchRequest | None = None) -> None:
            '''Delay execution of command (str) and request (_SearchRequest | None); return None.'''
            # Process later setting changes before entering the actual search.
            if request is not None:
                entered.set()
                self.assertTrue(self.search_release.wait(2.0))
            original(command, request)

        # Reading settings inside the search thread would incorrectly pick up new values here.
        with patch.object(self.engine, '_perform', side_effect=perform):
            self.commands.put('go byoyomi 60000\n')
            self.assertTrue(entered.wait(1.0))
            self.commands.put('setoption name Timelimit value 1000\n')
            self.commands.put('setoption name MultiPV value 3\n')
            self.commands.put('setoption name Threads value 2\n')
            self.commands.put('isready\n')
            self._wait_output('readyok')
            self.search_release.set()
            self._wait_output('bestmove ')
        self.assertEqual(self.player.evaluate.call_args.kwargs['timelimit'], 58.0)
        self.assertEqual(self.player.evaluate.call_args.kwargs['candidate_width'], 0)
        self.assertIs(self.engine.player, self.player)

    def test_run_ponder_updates_keep_settings(self) -> None:
        '''Keep old settings across ponder updates and change them only on the next go.; return
            None.'''
        # Use a position beyond move twenty to exercise extension setting changes.
        self._set_position(21)
        self.engine.ponder = True
        self._start_protocol()
        updating = Event()
        applied_max_visits = self.engine.max_visits

        def evaluate(**kwargs: object) -> list[Candidate]:
            '''Accept search arguments (kwargs: object), block the second update, and return
                candidates.
            Returns:
                list[Candidate]: Candidates returned after waiting for the update.
            '''
            # Ensure setoption is processed during a ponder update.
            self.search_started.set()
            if self.player.evaluate.call_count == 2:
                updating.set()
                self.assertTrue(self.search_release.wait(2.0))
            return self.candidates

        self.player.evaluate.side_effect = evaluate
        self.commands.put('go ponder\n')
        self._wait_output('info nodes')
        self.commands.put('setoption name MultiPV value 3\n')
        self.commands.put('setoption name Extends value 2\n')
        self.commands.put('setoption name MaxVisits value 200\n')
        self.commands.put('setoption name USI_Ponder value false\n')
        self.commands.put('setoption name USI_Hash value 1024\n')
        self.commands.put('isready\n')
        self._wait_output('readyok')
        self.assertTrue(updating.wait(2.0))
        self.assertFalse(self.stop_notified.is_set())
        for call in self.player.evaluate.call_args_list:
            self.assertEqual(call.kwargs['candidate_width'], 0)
            self.assertEqual(call.kwargs['extends'], 0)
            self.assertEqual(call.kwargs['visits'], applied_max_visits)
            self.assertTrue(call.kwargs['ponder'])
        self.assertFalse(self.engine.ponder)

        # After a ponderhit response, change only search settings for the next ponder request.
        self.commands.put('ponderhit\n')
        self._wait_output('bestmove ')
        calls = self.player.evaluate.call_count
        self.commands.put('go ponder\n')
        self.commands.put('isready\n')
        self._wait_output('readyok', 2)
        self.assertEqual(self.player.evaluate.call_count, calls)
        self.commands.put('stop\n')
        self._wait_output('bestmove ', 2)

        # Use the pending search settings when pondering is enabled again.
        updates = sum(line.startswith('info nodes') for line in self.output)
        self.commands.put('setoption name USI_Ponder value true\n')
        self.commands.put('go ponder\n')
        self._wait_output('info nodes', updates + 1)
        arguments = self.player.evaluate.call_args.kwargs
        self.assertEqual(arguments['candidate_width'], 3)
        self.assertEqual(arguments['extends'], 2)
        self.assertEqual(arguments['visits'], applied_max_visits)
        self.assertTrue(arguments['ponder'])

    def test_random_settings_fixed_until_next_go(self) -> None:
        '''Fix initial random move conditions and selection settings for each go too.; return
            None.'''
        # Use old settings for the current result even if random moves are disabled during search.
        self.engine.initial_turn = 4
        self.engine.initial_width = 5
        self.engine.initial_temperature = 0.5
        self.engine.initial_value_delta = 0.2
        self.engine.ponder = True
        self.candidates[0].variations = [
            usi_string_to_move(move) for move in ('7g7f', '3c3d')]
        self._start_protocol()
        self.player.get_random_candidate.side_effect = self._blocking_random_candidate
        self.commands.put('go byoyomi 10000\n')
        self.assertTrue(self.search_started.wait(1.0))
        for change in ('InitialTurn value 0', 'InitialWidth value 8',
                       'InitialTemperature value 100', 'InitialValueDelta value 10',
                       'InitialWhiteOnly value true', 'USI_Ponder value false'):
            self.commands.put(f'setoption name {change}\n')
        self.commands.put('isready\n')
        self._wait_output('readyok')
        self.assertFalse(self.stop_notified.is_set())
        self.search_release.set()
        self._wait_output('bestmove ')
        arguments = self.player.get_random_candidate.call_args.kwargs
        self.assertEqual(self.output[-1], 'bestmove 7g7f ponder 3c3d')
        self.assertEqual(arguments['width'], 5)
        self.assertEqual(arguments['temperature'], 0.5)
        self.assertEqual(arguments['delta'], 0.2)
        self.assertFalse(arguments['ponder'])
        self.assertFalse(self.engine.ponder)

        # Switch from random move selection to normal search on the next go.
        self.commands.put('go byoyomi 10000\n')
        self._wait_output('bestmove ', 2)
        self.player.get_random_candidate.assert_called_once()
        self.player.evaluate.assert_called_once()
        self.assertFalse(self.player.evaluate.call_args.kwargs['ponder'])
        self.assertEqual(self.output[-1], 'bestmove 7g7f')

    def _blocking_random_candidate(self, **kwargs: object) -> Candidate:
        '''Accept search arguments (kwargs: object) and return a candidate (Candidate) after
            release.
        Returns:
            Candidate: Candidate selected after the wait is released.
        '''
        # Use normal search's wait control to return only the random move result.
        return self._blocking_evaluation(**kwargs)[0]

    def test_run_quit_discards_result(self) -> None:
        '''Release waits on quit and exit without sending later search results.
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
        '''Keep ponder results and return them once after stop or ponderhit.
        '''
        self._start_protocol()
        cases = (
            (True, 'ponder', 'ponderhit'), (False, 'ponder', 'stop'),
            (False, 'ponder', 'ponderhit'), (False, 'infinite', 'stop'),
        )
        for count, (enabled, mode, command) in enumerate(cases, 1):
            # Wait for this search's result independently of the previous search's update count.
            updates = sum(line.startswith('info nodes') for line in self.output)
            calls = self.player.evaluate.call_count
            self.commands.put(f'setoption name USI_Ponder value {str(enabled).lower()}\n')
            self.commands.put(f'go {mode}\n')
            self.commands.put('isready\n')
            self._wait_output('readyok', count)
            if enabled or mode == 'infinite':
                self._wait_output('info nodes', updates + 1)
                self.assertTrue(self.player.evaluate.call_args.kwargs['ponder'])
                expected_move = '7g7f'
            else:
                self.assertEqual(self.player.evaluate.call_count, calls)
                expected_move = usi_move_to_string(
                    *self.player.get_board.return_value.get_legal_moves()[0])
            self.assertEqual(sum(line.startswith('bestmove ') for line in self.output), count - 1)
            self.commands.put(command + '\n')
            self._wait_output('bestmove ', count)
            self.assertEqual(self.output[-1], f'bestmove {expected_move}')
        self.assertEqual(sum(line.startswith('bestmove ') for line in self.output), len(cases))

    def test_run_terminal_ponder(self) -> None:
        '''Retain terminal pondering requests and return their result once on stop.
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
        '''Return a legal move without losing a request stopped just after its thread starts.
        '''
        self._start_protocol()
        entered = Event()
        original = self.engine._perform

        def perform(command: str, request: _SearchRequest | None = None) -> None:
            '''Delay execution of command (str) and request (_SearchRequest | None); return None.'''
            if request is not None:
                entered.set()
                self.assertTrue(self.search_release.wait(2.0))
            original(command, request)

        # Process stop after registering go but before entering the actual search.
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
        '''
        # Return the mate-search response without fetching the board or starting search.
        for args in ('mate 60000', 'mate infinite', 'mate'):
            with self.subTest(args=args):
                self.assertEqual(
                    self.engine._perform_command(f'go {args}'),
                    (True, 'checkmate notimplemented', False))
        self.player.get_board.assert_not_called()
        self.player.evaluate.assert_not_called()
