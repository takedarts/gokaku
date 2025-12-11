from typing import List, Tuple

from libc.stdint cimport int32_t
from libcpp cimport bool
from libcpp.string cimport string
from libcpp.vector cimport vector

include "board.pyx"
include "processor.pyx"

cdef extern from "cpp/Candidate.h" namespace "deepshogi":
    cdef cppclass Candidate:
        Move getMove() const
        int32_t getColor() const
        int32_t getVisits() const
        int32_t getPlayouts() const
        float getPolicy() const
        float getValue() const
        float getMinimax() const
        vector[Move] getVariations() const


cdef extern from "cpp/Player.h" namespace "deepshogi":
    cdef cppclass Player:
        Player(Processor*, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, bool, int32_t) except +
        void initialize(const string)
        int32_t getColor()
        void play(const Move&)
        void startEvaluation(bool, int32_t, int32_t, int32_t, float, float)
        void waitEvaluation(int32_t, int32_t, float, bool) nogil
        vector[Candidate] getCandidates()
        void copyBoardTo(Board*)
        string getDebugInfo()


cdef class NativePlayer:
    cdef Player* player
    def __cinit__(
        self,
        processor: NativeProcessor,
        threads: int,
        nyugyoku_scores: Tuple[int, int],
        draw_turn: int,
        check_search_depth: int,
        check_search_node: int,
        eval_leaf_only: bool,
        max_visits: int,
    )->None:
        '''Initialize player object.
        Args:
            processor (NativeProcessor): Processor object
            threads (int): Number of threads
            nyugyoku_scores (Tuple[int, int]): Score required for nyugyoku declaration
            draw_turn (int): Number of moves for a draw
            check_search_depth (int): Depth for checkmate search
            check_search_node (int): Number of nodes for checkmate search
            eval_leaf_only (bool): True to evaluate only leaf nodes
            max_visits (int): Maximum number of visits for search
        '''
        self.player = new Player(
            processor.processor, threads,
            nyugyoku_scores[0], nyugyoku_scores[1], draw_turn,
            check_search_depth, check_search_node, eval_leaf_only, max_visits)

    def __dealloc__(self):
        del self.player

    def initialize(self, sfen: str) -> None:
        '''Set the game state to the initial state.
        Args:
            sfen (str): SFEN string of the position
        '''
        self.player.initialize(sfen.encode('utf-8'))

    def get_color(self) -> int:
        '''Get side to move.
        Returns:
            int: Side to move
        '''
        return self.player.getColor()

    def play(self, src: Tuple[int, int], dst: Tuple[int, int], promote: bool) -> int:
        '''Move a piece.
        Args:
            src (Tuple[int, int]): Source coordinates
            dst (Tuple[int, int]): Destination coordinates
            promote (bool): True if promote
        '''
        self.player.play(Move(src[0], src[1], dst[0], dst[1], promote))

    def start_evaluation(
        self,
        equally: bool,
        algorithm: int,
        candidate_width: int,
        check_node_depth: int,
        temperature: float,
        noise: float,
    ) -> None:
        '''Start evaluation.
        Args:
            equally (bool): True to make search count equal, False to use UCB or PUCB
            algorithm (int): Search algorithm
            candidate_width (int): Search width for candidate moves (0 for automatic setting)
            check_node_depth (int): Maximum depth for checkmate search nodes
            temperature (float): Temperature parameter for search
            noise (float): Strength of Gumbel noise for search
        '''
        self.player.startEvaluation(
            equally, algorithm, candidate_width, check_node_depth, temperature, noise)

    def wait_evaluation(self, visits: int, playouts: int, timelimit: float, stop: bool) -> None:
        '''Wait until the specified number of visits and playouts is reached.
        Args:
            visits (int): Number of visits
            playouts (int): Number of playouts
            timelimit (float): Time limit (seconds)
            stop (bool): True to issue stop command
        '''
        cdef int32_t visits_int = visits
        cdef int32_t playouts_int = playouts
        cdef float timelimit_float = timelimit
        cdef bool stop_bool = stop

        with nogil:
            self.player.waitEvaluation(visits_int, playouts_int, timelimit_float, stop_bool)

    def get_candidates(
        self,
    ) -> List[Tuple[Tuple[int, int], Tuple[int, int], int, int, int, float, float, float, List[int]]]:
        '''Get list of candidate moves.
        Returns:
            List[Tuple[Tuple[int, int], Tuple[int, int], int, int, int, float, float, float, List[int]]]: List of candidate moves
        '''
        cdef vector[Candidate] candidates = self.player.getCandidates()

        results: List[Tuple[Tuple[int, int], Tuple[int, int], int, int, float, float, float, List[int]]] = []

        for i in range(candidates.size()):
            src = (candidates[i].getMove().getSrcX(), candidates[i].getMove().getSrcY())
            dst = (candidates[i].getMove().getDstX(), candidates[i].getMove().getDstY())
            promote = candidates[i].getMove().isPromote()
            color = candidates[i].getColor()
            visits = candidates[i].getVisits()
            playouts = candidates[i].getPlayouts()
            policy = candidates[i].getPolicy()
            value = candidates[i].getValue()
            minimax = candidates[i].getMinimax()
            variations = candidates[i].getVariations()

            results.append((
                src, dst, promote, color, visits, playouts, policy, value, minimax,
                [variations[j].getValue() for j in range(variations.size())],
            ))

        return results

    def copy_board_to(self, board: NativeBoard) -> None:
        '''Copy the board state to the specified board object.
        Args:
            board (NativeBoard): Board object
        '''
        self.player.copyBoardTo(board.board)

    def get_debug_info(self) -> str:
        '''Output debug information of the search tree.
        Returns:
            str: Debug information
        '''
        return self.player.getDebugInfo().decode('utf-8')
