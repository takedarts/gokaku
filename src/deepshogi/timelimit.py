'''Calculate thinking time from the remaining time and game rules.'''


def get_timelimit(
    remain_time: float,
    byoyomi_time: float,
    increment_time: float,
    current_moves: int,
    draw_turn: int | None = None,
    *,
    delay_time: float = 0.0,
    move_cost: float = 1.0,
    clock_limit: float | None = None,
) -> float:
    '''Return the time available for the current move in seconds.

    Time inputs are nonnegative; remaining time excludes the current increment.
    Reserve time for future moves and subtract a margin based on the delay plus byoyomi.
    Args:
        remain_time (float): Remaining time before the current increment, in seconds.
        byoyomi_time (float): Byoyomi time available per move, in seconds.
        increment_time (float): Increment available starting with this move, in seconds.
        current_moves (int): Total moves already played by both sides.
        draw_turn (int | None): Total move limit for a draw, or None if unspecified.
        delay_time (float): Delay time per move, in seconds.
        move_cost (float): Expected time consumed by each of our future moves, in seconds.
        clock_limit (float | None): Game-clock limit before subtracting the margin, in seconds,
            or None if unspecified.
    Returns:
        float: Nonnegative thinking time limit for the current move, in seconds.
    '''
    # Exclude this move when counting future moves; default to one hundred.
    if draw_turn is None:
        remaining_moves = 100
    else:
        remaining_moves = max((draw_turn - current_moves - 1) // 2, 0)

    # Reserve time when byoyomi and increment cannot cover the expected time per move.
    each_time = byoyomi_time + increment_time

    if each_time < move_cost:
        required_time = max(move_cost - increment_time, 0.0)
    else:
        required_time = 0.0

    # Subtract the future reserve without allowing negative available time.
    available_time = max(remain_time - required_time * remaining_moves, 0.0)

    # Add the delay and byoyomi and limit the time for this move by the game-clock limit.
    free_time = delay_time + byoyomi_time
    available_time = available_time + free_time + increment_time

    if clock_limit is not None:
        available_time = min(available_time, clock_limit)

    # Subtract a margin based on the time available every move besides the main time.
    margin_time = 2.0 if free_time >= 2.0 else 10.0

    return max(available_time - margin_time, 0.0)
