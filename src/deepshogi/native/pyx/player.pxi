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
        check_search_depth: int,
        check_search_node: int,
        check_node_depth: int,
        pucb_constant_init: float,
        pucb_constant_base: float,
    )->None:
        '''Initializes the player object.
        Args:
            processor (NativeInferenceProcessor): Inference processor object
            threads (int): Number of threads
            max_visits (int): Maximum number of visits per node
            nyugyoku_scores (Tuple[int, int]): Scores required for entering-king declaration
            draw_turn (int): Number of moves until a draw
            check_search_depth (int): Depth of the checkmate search
            check_search_node (int): Number of nodes in the checkmate search
            check_node_depth (int): Depth of nodes in the checkmate search
            pucb_constant_init (float): Initial value of the constant multiplied by the PUCB confidence bound
            pucb_constant_base (float): Rate of change of the constant multiplied by the PUCB confidence bound
        '''
        self.player = new Player(
            processor.processor, threads, max_visits,
            nyugyoku_scores[0], nyugyoku_scores[1], draw_turn,
            check_search_depth, check_search_node, check_node_depth,
            pucb_constant_init, pucb_constant_base)

    def __dealloc__(self):
        del self.player

    def initialize(self, sfen: str) -> None:
        '''Sets the game state to the initial state.
        Args:
            sfen (str): Board position in SFEN format
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
        '''Starts evaluation.
        Args:
            equally (bool): True to distribute visits equally, False to use PUCB or similar
            candidate_width (int): Search width for candidate moves (0 to set automatically)
            temperature (float): Temperature parameter for the search
            noise (float): Strength of Gumbel noise in the search
        '''
        self.player.startEvaluation(equally, candidate_width, temperature, noise)

    def wait_evaluation(self, visits: int, playouts: int, timelimit: float, stop: bool) -> None:
        '''Waits until the specified visit count and playout count are reached.
        Args:
            visits (int): Number of visits
            playouts (int): Number of playouts
            timelimit (float): Time limit in seconds
            stop (bool): True to issue a stop command
        '''
        cdef int32_t visits_int = visits
        cdef int32_t playouts_int = playouts
        cdef float timelimit_float = timelimit
        cdef bool stop_bool = stop

        with nogil:
            self.player.waitEvaluation(visits_int, playouts_int, timelimit_float, stop_bool)

    def get_candidates(
        self,
    ) -> List[Tuple[Tuple[int, int], Tuple[int, int], int, int, int, float, float, List[int]]]:
        '''Returns the list of candidate moves.
        Returns:
            List[Tuple[Tuple[int, int], Tuple[int, int], int, int, int, float, float, List[int]]]: List of candidate moves
        '''
        cdef vector[Candidate] candidates = self.player.getCandidates()

        results: List[Tuple[Tuple[int, int], Tuple[int, int], int, int, float, float, List[int]]] = []

        for i in range(candidates.size()):
            src = (candidates[i].getMove().getSrc().getX(), candidates[i].getMove().getSrc().getY())
            dst = (candidates[i].getMove().getDst().getX(), candidates[i].getMove().getDst().getY())
            promote = candidates[i].getMove().isPromote()
            color = candidates[i].getColor()
            visits = candidates[i].getVisits()
            playouts = candidates[i].getPlayouts()
            policy = candidates[i].getPolicy()
            value = candidates[i].getValue()
            variations = candidates[i].getVariations()

            results.append((
                src, dst, promote, color, visits, playouts, policy, value,
                [variations[j].getValue() for j in range(variations.size())],
            ))

        return results

    def get_visits(self) -> int:
        '''Gets the number of visits to the root node.
        Returns:
            int: Number of visits to the root node
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
