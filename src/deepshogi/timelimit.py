'''Calculate thinking time from the remaining time and game rules.'''


def get_timelimit(
    remain_time: float,
    byoyomi_time: float,
    increment_time: float,
    current_moves: int,
    draw_turn: int | None = None,
) -> float:
    '''Return the time available for the current move in seconds.

    Time inputs are nonnegative; remaining time excludes the current increment.
    Reserve time for future moves and leave a one-second margin for this move.
    Args:
        remain_time (float): Remaining time before the current increment, in seconds.
        byoyomi_time (float): Byoyomi time available per move, in seconds.
        increment_time (float): Increment available starting with this move, in seconds.
        current_moves (int): Total moves already played by both sides.
        draw_turn (int | None): Total move limit for a draw, or None if unspecified.
    Returns:
        float: Nonnegative thinking time limit for the current move, in seconds.
    '''
    # Exclude this move when counting future moves; default to one hundred.
    if draw_turn is None:
        remaining_moves = 100
    else:
        remaining_moves = max((draw_turn - current_moves - 1) // 2, 0)

    # Reserve time when byoyomi and increment cannot cover one second per move.
    each_time = byoyomi_time + increment_time

    if each_time < 1.0:
        required_time = max(1.0 - increment_time, 0.0)
    else:
        required_time = 0.0

    # Subtract the future reserve without allowing negative available time.
    available_time = max(remain_time - required_time * remaining_moves, 0.0)

    # Include this move's byoyomi and increment, leaving a one-second margin.
    return max(available_time + byoyomi_time + increment_time - 1.0, 0.0)
