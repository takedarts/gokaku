from typing import List, Tuple

cimport cython
from libc.stdint cimport int32_t
from libcpp cimport bool
from libcpp.vector cimport vector
from deepshogi.validation import validate_positive_float, validate_positive_int
from pyx.candidate cimport Candidate
from pyx.move cimport Move
from pyx.player cimport Player
from pyx.position cimport Position


@cython.no_gc_clear
cdef class NativePlayer:
    '''Manage game state and choose moves as a player.
    Keep the inference processor alive until the native player has been destroyed.
    '''
    cdef Player* player
    cdef NativeInferenceProcessor _processor # type: ignore

    def __cinit__(
        self,
        processor: NativeInferenceProcessor,  # type: ignore
        threads: int,
        max_visits: int,
        nyugyoku_scores: Tuple[int, int],
        sennichite_penalty: float,
        check_search_depth: int,
        check_search_node: int,
        check_node_depth: int,
        pucb_constant_init: float,
        pucb_constant_base: float,
        pucb_min_visits_rate: float,
    )->None:
        '''Initialize the native player.
        Args:
            processor (NativeInferenceProcessor): Inference processor.
            threads (int): Number of search threads.
            max_visits (int): Maximum node visit count.
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
        '''
        # Reject implicit conversions of fractions and booleans in direct native API calls.
        validate_positive_int('threads', threads)
        validate_positive_int('max_visits', max_visits)
        validate_positive_float('pucb_constant_base', pucb_constant_base)

        # Retain the processor until destruction of the C++ player is complete.
        self._processor = processor

        # Construct the C++ player.
        self.player = new Player(
            processor.processor, threads, max_visits,
            nyugyoku_scores[0], nyugyoku_scores[1],
            sennichite_penalty, check_search_depth, check_search_node, check_node_depth,
            pucb_constant_init, pucb_constant_base, pucb_min_visits_rate)

    def __dealloc__(self) -> None:
        '''Destroy the native player; return None.'''
        # Cython releases the retained processor reference after this method completes.
        del self.player

    def initialize(self, sfen: str, draw_turn: int) -> None:
        '''Reset the game to the initial position.
        Args:
            sfen (str): Position in SFEN format.
            draw_turn (int): Total move count at which the game is drawn.
        '''
        self.player.initialize(sfen.encode('utf-8'), draw_turn)

    def get_color(self) -> int:
        '''Returns the current turn color.
        Returns:
            int: Current turn color
        '''
        return self.player.getColor()

    def play(self, src: Tuple[int, int], dst: Tuple[int, int], promote: bool) -> None:
        '''Moves a piece.
        Args:
            src (Tuple[int, int]): Source coordinate
            dst (Tuple[int, int]): Destination coordinate
            promote (bool): True if promoting
        '''
        cdef Position src_pos = Position(src[0], src[1])
        cdef Position dst_pos = Position(dst[0], dst[1])
        cdef Move move = Move(src_pos, dst_pos, promote)
        self.player.play(move)

    def start_evaluation(
        self,
        equally: bool,
        candidate_width: int,
        temperature: float,
        noise: float,
    ) -> None:
        '''Start evaluation.
        Args:
            equally (bool): True for equal visits; False for PUCB or similar selection.
            candidate_width (int): Candidate search width; zero selects it automatically.
            temperature (float): Search temperature.
            noise (float): Strength of Gumbel noise during search.
        '''
        cdef bool equally_bool = equally
        cdef int32_t width_int = candidate_width
        cdef float temperature_float = temperature
        cdef float noise_float = noise

        # Allow USI input while waiting for active tasks to finish.
        with nogil:
            self.player.startEvaluation(
                equally_bool, width_int, temperature_float, noise_float)

    def stop_evaluation(self) -> None:
        '''Request stopping and wake waiting threads.
        Returns:
            None: No return value.
        '''
        # Allow other Python threads to run while notifying the C++ search.
        with nogil:
            self.player.stopEvaluation()

    def wait_evaluation(
        self,
        visits: int,
        timelimit: float,
        stop: bool,
    ) -> None:
        '''Wait until the requested visit count is reached.
        Args:
            visits (int): Visit count.
            timelimit (float): Time limit in seconds.
            stop (bool): True to request stopping.
        '''
        cdef int32_t visits_int = visits
        cdef float timelimit_float = timelimit
        cdef bool stop_bool = stop

        with nogil:
            self.player.waitEvaluation(visits_int, timelimit_float, stop_bool)

    def get_candidates(
        self,
    ) -> List[Tuple[Tuple[int, int], Tuple[int, int], bool, int, int, float, float,
                    float, List[int]]]:
        '''Return the candidate moves.
        Returns:
            List[Tuple[Tuple[int, int], Tuple[int, int], bool, int, int, float, float,
                       float, List[int]]]: Candidate moves.
        '''
        cdef vector[Candidate] candidates

        # Allow stop and quit commands while draining search tasks.
        with nogil:
            candidates = self.player.getCandidates()

        results: List[Tuple[Tuple[int, int], Tuple[int, int], bool, int, int, float,
                            float, float, List[int]]] = []

        for i in range(candidates.size()):
            src = (candidates[i].getMove().getSrc().getX(), candidates[i].getMove().getSrc().getY())
            dst = (candidates[i].getMove().getDst().getX(), candidates[i].getMove().getDst().getY())
            promote = candidates[i].getMove().isPromote()
            color = candidates[i].getColor()
            visits = candidates[i].getVisits()
            policy = candidates[i].getPolicy()
            value = candidates[i].getValue()
            remaining_turns = candidates[i].getRemainingTurns()
            variations = candidates[i].getVariations()

            results.append((
                src, dst, promote, color, visits, policy, value, remaining_turns,
                [variations[j].getValue() for j in range(variations.size())],
            ))

        return results

    def get_visits(self) -> int:
        '''Return the root node's visit count.
        Returns:
            int: Root node visit count.
        '''
        return self.player.getVisits()

    def copy_board_to(self, board: NativeBoard) -> None:  # type: ignore
        '''Copies the board state to the specified board object.
        Args:
            board (NativeBoard): Board object
        '''
        self.player.copyBoardTo(board.board)

    def to_string(self) -> str:
        '''Returns a string representing the state of the player object.
        Returns:
            str: String representing the state of the player object
        '''
        return self.player.toString().decode('utf-8')
