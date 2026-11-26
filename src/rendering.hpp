#pragma once

#include "simulation.hpp"

#include <SDL3/SDL.h>

namespace rendering {

inline constexpr int WINDOW_WIDTH = 1280;
inline constexpr int WINDOW_HEIGHT = 720;

struct UiState {
  int selectedFloor = MIN_FLOOR;
  ElevatorId selectedElevator = 0;
  bool showHelp = false;
  bool paused = false;
};

void renderFrame(SDL_Renderer *renderer, const UiState &uiState,
                 const SimulationSnapshot &snapshot);

} // namespace rendering
