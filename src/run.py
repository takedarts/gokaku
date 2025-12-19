import argparse
import sys

import torch
from deepshogi.config import (DEFAULT_CHECK_NODE_DEPTH,
                              DEFAULT_CHECK_SEARCH_DEPTH,
                              DEFAULT_CHECK_SEARCH_NODE, DEFAULT_DRAW_TURN,
                              DEFAULT_PUCB_CONSTANT_BASE,
                              DEFAULT_PUCB_CONSTANT_INIT, DEFAULT_UCB_CONSTANT,
                              NAME, VERSION)
from deepshogi.gpu import get_default_gpus
from deepshogi.log import start_logging
from deepshogi.processor import Processor
from deepshogi.usi import USIEngine


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description='Run with GTP mode.')
    parser.add_argument(
        'model', type=str, help='Path to the model file')
    parser.add_argument(
        '--visits', type=int, default=50, help='Number of visits (default: 50)')
    parser.add_argument(
        '--playouts', type=int, default=0, help='Number of playouts (default: 0)')
    parser.add_argument(
        '--timelimit', type=float, default=120.0, help='Time to think (sec) (default: 120)')
    parser.add_argument(
        '--search', type=str, default='pucb', choices=['ucb', 'pucb'],
        help='Calculation method of search (default: pucb)')
    parser.add_argument(
        '--criterion', type=str, default='value', choices=['value', 'minimax', 'visits'],
        help='Criterion for candidate prioritization (default: value)')
    parser.add_argument(
        '--ponder', default=False, action='store_true', help='Use pondering')
    parser.add_argument(
        '--resign', type=float, default=0.02, help='Resign threshold (default: 0.02)')
    parser.add_argument(
        '--min-turn', type=int, default=50, help='Minimum number of resign turns (default: 50)')
    parser.add_argument(
        '--initial-turn', type=int, default=4, help='Number of turns to move randomly (default: 4)')
    parser.add_argument(
        '--initial-width', type=int, default=16, help='Search width for random moves (default: 16)')
    parser.add_argument(
        '--nyugyoku-rule', type=str, default='27', choices=['27', '24'], help='Nyugyoku rule (default: 27)')
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
        '--ucb-constant', type=float, default=DEFAULT_UCB_CONSTANT,
        help=f'Constant value in UCB (default: {DEFAULT_UCB_CONSTANT})')
    parser.add_argument(
        '--pucb-constant-init', type=float, default=DEFAULT_PUCB_CONSTANT_INIT,
        help=f'Initial value of the constant in PUCB (default: {DEFAULT_PUCB_CONSTANT_INIT})')
    parser.add_argument(
        '--pucb-constant-base', type=float, default=DEFAULT_PUCB_CONSTANT_BASE,
        help=f'Change value of the constant in PUCB (default: {DEFAULT_PUCB_CONSTANT_BASE})')
    parser.add_argument(
        '--client-name', type=str, default=NAME, help=f'Client name (default: {NAME})')
    parser.add_argument(
        '--client-version', type=str, default=VERSION, help=f'Client version (default: {VERSION})')
    parser.add_argument(
        '--threads', type=int, default=16, help='Number of threads (default: 16)')
    parser.add_argument(
        '--batch-size', type=int, default=2048, help='Batch size (default: 2048)')
    parser.add_argument(
        '--gpus', type=lambda x: list(map(int, x.split(','))), default=None,
        help='GPU IDs (comma-separated) (default: all available GPUs)')
    parser.add_argument(
        '--fp16', default=False, action='store_true', help='Use FP16')
    parser.add_argument(
        '--verbose', action='store_true', help='Verbose mode')

    args = parser.parse_args()
    args.gpus, args.fp16 = get_default_gpus(args.gpus, args.fp16)

    return args


def main() -> None:
    args = parse_args()

    # Set up log output
    start_logging(debug=args.verbose, console=sys.stderr)

    # Configure GPU settings
    if torch.cuda.device_count() != 0:
        torch.backends.cudnn.enabled = True
        torch.backends.cudnn.benchmark = True
        torch.backends.cudnn.deterministic = False

    # Create inference object
    processor = Processor(args.model, args.gpus, args.batch_size, args.fp16)

    # Create GPT object
    engine = USIEngine(
        processor=processor,
        threads=args.threads,
        visits=args.visits,
        playouts=args.playouts,
        timelimit=args.timelimit,
        algorithm=args.search,
        criterion=args.criterion,
        ponder=args.ponder,
        resign_threshold=args.resign,
        resign_turn=args.min_turn,
        initial_turn=args.initial_turn,
        initial_width=args.initial_width,
        nyugyoku_scores=(31, 31) if args.nyugyoku_rule == '24' else (28, 27),
        draw_turn=args.draw_turn,
        check_search_depth=args.check_search_depth,
        check_search_node=args.check_search_node,
        check_node_depth=args.check_node_depth,
        ucb_constant=args.ucb_constant,
        pucb_constant_init=args.pucb_constant_init,
        pucb_constant_base=args.pucb_constant_base,
        client_name=args.client_name,
        client_version=args.client_version,
    )

    # Run the game
    engine.run()


if __name__ == '__main__':
    main()
