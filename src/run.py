import argparse
import sys
from contextlib import ExitStack
from functools import partial

from deepshogi.config import (DEFAULT_CHECK_NODE_DEPTH, DEFAULT_CHECK_SEARCH_DEPTH,
                              DEFAULT_CHECK_SEARCH_NODE, DEFAULT_DRAW_TURN, DEFAULT_MAX_VISITS,
                              DEFAULT_PUCB_CONSTANT_BASE, DEFAULT_PUCB_CONSTANT_INIT,
                              DEFAULT_PUCB_MIN_VISITS_RATE, DEFAULT_THREADS_PER_GPU, NAME, VERSION)
from deepshogi.gpu import parse_gpu_ids
from deepshogi.loader import open_processor
from deepshogi.log import start_logging
from deepshogi.processor import Processor
from deepshogi.usi import USIEngine
from deepshogi.validation import validate_nonnegative_float, validate_positive_int


def parse_args() -> argparse.Namespace:
    '''Parse the USI engine startup arguments before opening the model.
    Returns:
        argparse.Namespace: Startup settings with visits and cache size normalized.
    '''
    parser = argparse.ArgumentParser(description='Run with USI mode.')
    parser.add_argument(
        'model', type=str, help='Path to an ExportedProgram (.ep) or a compiled model (.pt2).')
    parser.add_argument(
        '--visits', type=int, default=50,
        help='Number of visits (default: 50)')
    parser.add_argument(
        '--extends', type=int, default=0,
        help='Maximum number of search extensions (default: 0)')
    parser.add_argument(
        '--max-visits', type=int, default=DEFAULT_MAX_VISITS,
        help=f'Maximum number of visits (default: {DEFAULT_MAX_VISITS})')
    parser.add_argument(
        '--timelimit', type=float, default=120.0,
        help='Time to think (sec) (default: 120)')
    parser.add_argument(
        '--criterion', type=str, default='value', choices=['value', 'visits'],
        help='Criterion for candidate prioritization (default: value)')
    parser.add_argument(
        '--ponder', default=False, action='store_true',
        help='Use pondering')
    parser.add_argument(
        '--resign', type=float, default=0.02,
        help='Resign threshold (default: 0.02)')
    parser.add_argument(
        '--min-turn', type=int, default=50,
        help='Minimum number of resign turns (default: 50)')
    parser.add_argument(
        '--initial-turn', type=int, default=4,
        help='Number of turns to move randomly (default: 4)')
    parser.add_argument(
        '--initial-width', type=int, default=16,
        help='Search width for random moves (default: 16)')
    parser.add_argument(
        '--initial-temperature', type=float, default=1.0,
        help='Temperature parameter for random moves (default: 1.0)')
    parser.add_argument(
        '--initial-value-delta', type=float, default=0.01,
        help='Maximum win probability drop for random moves (finite and >= 0, default: 0.01)')
    parser.add_argument(
        '--initial-white-only', default=False, action='store_true',
        help='Only move randomly when playing as white')
    parser.add_argument(
        '--nyugyoku-rule', type=str, default='27', choices=['27', '24'],
        help='Nyugyoku rule (default: 27)')
    parser.add_argument(
        '--draw-turn', type=int, default=DEFAULT_DRAW_TURN,
        help=f'Number of turns to declare a draw (default: {DEFAULT_DRAW_TURN})')
    parser.add_argument(
        '--check-search-depth', type=int, default=DEFAULT_CHECK_SEARCH_DEPTH,
        help=f'Check search depth (default: {DEFAULT_CHECK_SEARCH_DEPTH})')
    parser.add_argument(
        '--check-search-node', type=int, default=DEFAULT_CHECK_SEARCH_NODE,
        help=f'Check search node (default: {DEFAULT_CHECK_SEARCH_NODE})')
    parser.add_argument(
        '--check-node-depth', type=int, default=DEFAULT_CHECK_NODE_DEPTH,
        help=f'Node depth where check search runs (default: {DEFAULT_CHECK_NODE_DEPTH})')
    parser.add_argument(
        '--pucb-constant-init', type=float, default=DEFAULT_PUCB_CONSTANT_INIT,
        help=f'Initial value of the constant in PUCB (default: {DEFAULT_PUCB_CONSTANT_INIT})')
    parser.add_argument(
        '--pucb-constant-base', type=float, default=DEFAULT_PUCB_CONSTANT_BASE,
        help=f'Change value of the constant in PUCB (default: {DEFAULT_PUCB_CONSTANT_BASE})')
    parser.add_argument(
        '--pucb-min-visits-rate', type=float, default=DEFAULT_PUCB_MIN_VISITS_RATE,
        help=f'Minimum visits rate in PUCB (default: {DEFAULT_PUCB_MIN_VISITS_RATE})')
    parser.add_argument(
        '--client-name', type=str, default=NAME,
        help=f'Client name (default: {NAME})')
    parser.add_argument(
        '--client-version', type=str, default=VERSION,
        help=f'Client version (default: {VERSION})')
    parser.add_argument(
        '--threads', type=int, default=16,
        help='Number of threads (default: 16)')
    parser.add_argument(
        '--gpus', type=parse_gpu_ids, default=None,
        help='GPU IDs (comma-separated), or -1 for CPU. EP compilation uses the first ID '
             '(default: all GPUs of the same model as the first for EP, '
             'all compatible devices for PT2)')
    parser.add_argument(
        '--threads-per-gpu', type=int, default=DEFAULT_THREADS_PER_GPU,
        help=f'Number of threads per GPU (default: {DEFAULT_THREADS_PER_GPU})')
    parser.add_argument(
        '--cache-size', type=int, default=None,
        help='Cache size for board evaluation (default: visits)')
    parser.add_argument(
        '--verbose', action='store_true',
        help='Verbose mode')

    args = parser.parse_args()

    # Validate input settings before normalization or native construction.
    try:
        for name in ('threads', 'max_visits', 'threads_per_gpu'):
            validate_positive_int(name, getattr(args, name))
        validate_nonnegative_float('--initial-value-delta', args.initial_value_delta)

        # Ensure the maximum visits adjusted by the visit count fits in the native integer.
        args.max_visits = max(args.max_visits, args.visits)
        validate_positive_int('max_visits', args.max_visits)
    except ValueError as error:
        parser.error(str(error))

    # Use the requested visit count when no cache size is specified.
    if args.cache_size is None:
        args.cache_size = args.visits

    return args


def create_processor(args: argparse.Namespace, stack: ExitStack) -> Processor:
    '''Open the processor at isready and keep the model until the USI loop ends.
    Args:
        args (argparse.Namespace): Model path, devices, thread count, and cache settings.
        stack (ExitStack): Stack that closes the model context when the USI loop ends.
    Returns:
        Processor: Processor reused across games.
    '''
    # If initialization fails, the next isready opens a new context.
    return stack.enter_context(open_processor(
        args.model, gpus=args.gpus,
        threads_per_gpu=args.threads_per_gpu,
        cache_size=args.cache_size))


def main() -> None:
    '''Start the USI engine, which prepares the processor at the first isready.'''
    args = parse_args()

    # Configure logging.
    start_logging(debug=args.verbose, console=sys.stderr)

    # Respond to usi first, and keep the model opened at isready until the search ends.
    with ExitStack() as stack:
        engine = USIEngine(
            processor=None,
            processor_factory=partial(create_processor, args, stack),
            threads=args.threads,
            visits=args.visits,
            extends=args.extends,
            timelimit=args.timelimit,
            criterion=args.criterion,
            ponder=args.ponder,
            max_visits=args.max_visits,
            resign_threshold=args.resign,
            resign_turn=args.min_turn,
            initial_turn=args.initial_turn,
            initial_width=args.initial_width,
            initial_temperature=args.initial_temperature,
            initial_value_delta=args.initial_value_delta,
            initial_white_only=args.initial_white_only,
            nyugyoku_scores=(31, 31) if args.nyugyoku_rule == '24' else (28, 27),
            draw_turn=args.draw_turn,
            check_search_depth=args.check_search_depth,
            check_search_node=args.check_search_node,
            check_node_depth=args.check_node_depth,
            pucb_constant_init=args.pucb_constant_init,
            pucb_constant_base=args.pucb_constant_base,
            pucb_min_visits_rate=args.pucb_min_visits_rate,
            client_name=args.client_name,
            client_version=args.client_version,
        )
        engine.run()


if __name__ == '__main__':
    main()
