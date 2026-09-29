from typing import List, Tuple

from libc.stdint cimport int32_t
from libcpp cimport bool
from libcpp.vector cimport vector
from pyx.candidate cimport Candidate
from pyx.move cimport Move
from pyx.player cimport Player
from pyx.position cimport Position


cdef class NativePlayer:
    '''Class that manages game state and decides moves as a player.'''
    cdef Player* player

    def __cinit__(
        self,
        processor: NativeInferenceProcessor,  # type: ignore
        threads: int,
        max_visits: int,
        nyugyoku_scores: Tuple[int, int],
        draw_turn: int,
        sennichite_penalty: float,
        check_search_depth: int,
        check_search_node: int,
        check_node_depth: int,
        pucb_constant_init: float,
        pucb_constant_base: float,
        pucb_min_visits_rate: float,
    )->None:
        '''Initialize the player.
        Args:
            processor (NativeInferenceProcessor): Native inference processor.
            threads (int): Number of threads.
            max_visits (int): Maximum node visit count.
            nyugyoku_scores (Tuple[int, int]): Points required for entering-king declarations.
            draw_turn (int): Move count at which the game is drawn.
            sennichite_penalty (float): Penalty assigned to repetition evaluations.
            check_search_depth (int): Checkmate search depth.
            check_search_node (int): Checkmate search node capacity.
            check_node_depth (int): Node depth at which to run checkmate search.
            pucb_constant_init (float): Initial PUCB exploration coefficient.
            pucb_constant_base (float): Base controlling the PUCB exploration coefficient.
            pucb_min_visits_rate (float): Minimum visit ratio for prioritizing PUCB children.
        '''
        self.player = new Player(
            processor.processor, threads, max_visits,
            nyugyoku_scores[0], nyugyoku_scores[1], draw_turn,
            sennichite_penalty, check_search_depth, check_search_node, check_node_depth,
            pucb_constant_init, pucb_constant_base, pucb_min_visits_rate)

    def __dealloc__(self):
        del self.player

    def initialize(self, sfen: str) -> None:
        '''Reset the game to the initial position.
        Args:
            sfen (str): Position in SFEN format.
        '''
        self.player.initialize(sfen.encode('utf-8'))

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
        self.player.startEvaluation(equally, candidate_width, temperature, noise)

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
        cdef vector[Candidate] candidates = self.player.getCandidates()

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
