#pragma once

namespace simulation_timing {

inline constexpr int TICKS_PER_SECOND = 20;

inline constexpr double SECONDS_PER_TICK = 1.0 / TICKS_PER_SECOND;

inline constexpr int TRAVEL_TICKS_PER_FLOOR = 3 * TICKS_PER_SECOND;
inline constexpr int DOOR_OPENING_TICKS = 1 * TICKS_PER_SECOND;
inline constexpr int DOOR_OPEN_TICKS = 2 * TICKS_PER_SECOND;
inline constexpr int DOOR_CLOSING_TICKS = 1 * TICKS_PER_SECOND;

} // namespace simulation_timing
