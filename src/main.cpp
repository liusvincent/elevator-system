#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "rendering.hpp"
#include "simulation.hpp"
#include "simulation_timing.hpp"

#include <algorithm>
#include <random>

enum class AppMode { Manual, Simulation };

constexpr int NUM_ELEVATORS = 3;
constexpr double NANOSECONDS_PER_SECOND = 1'000'000'000.0;
constexpr double MAX_FRAME_TIME_SECONDS = 0.25;

static SDL_Window *window = nullptr;
static SDL_Renderer *renderer = nullptr;
static Simulation simulation{NUM_ELEVATORS};
static rendering::UiState ui;
static AppMode activeMode = AppMode::Manual;
static bool showModeMenu = false;
static Uint64 previousTimeNs = 0;
static double accumulatorSeconds = 0.0;
static std::mt19937 passengerRandom{std::random_device{}()};

int randomDestination(int originFloor, Direction direction) {
  if (direction == Direction::Up) {
    std::uniform_int_distribution<int> destination(originFloor + 1,
                                                   MAX_FLOOR);
    return destination(passengerRandom);
  }

  std::uniform_int_distribution<int> destination(MIN_FLOOR,
                                                 originFloor - 1);
  return destination(passengerRandom);
}

void switchMode(AppMode mode) {
  if (mode != activeMode) {
    activeMode = mode;
    simulation = Simulation{NUM_ELEVATORS};
    ui = rendering::UiState{};
  }

  showModeMenu = false;
  accumulatorSeconds = 0.0;
}

void handleSharedControls(SDL_Keycode key) {
  if (key >= SDLK_1 && key <= SDLK_8) {
    ui.selectedFloor = static_cast<int>(key - SDLK_1) + MIN_FLOOR;
    return;
  }

  switch (key) {
  case SDLK_UP:
    if (ui.selectedFloor < MAX_FLOOR) {
      ++ui.selectedFloor;
    }
    break;

  case SDLK_DOWN:
    if (ui.selectedFloor > MIN_FLOOR) {
      --ui.selectedFloor;
    }
    break;

  case SDLK_SPACE:
    ui.paused = !ui.paused;
    break;

  case SDLK_N:
    if (ui.paused) {
      simulation.tick();
    }
    break;
  }
}

void handleManualControls(SDL_Keycode key) {
  switch (key) {
  case SDLK_LEFT:
    if (ui.selectedElevator > 0) {
      --ui.selectedElevator;
    }
    break;

  case SDLK_RIGHT:
    if (ui.selectedElevator + 1 < simulation.elevatorCount()) {
      ++ui.selectedElevator;
    }
    break;

  case SDLK_U:
    if (ui.selectedFloor < MAX_FLOOR) {
      simulation.addHallCall({ui.selectedFloor, Direction::Up});
    }
    break;

  case SDLK_D:
    if (ui.selectedFloor > MIN_FLOOR) {
      simulation.addHallCall({ui.selectedFloor, Direction::Down});
    }
    break;

  case SDLK_E:
    simulation.selectFloor(ui.selectedElevator, ui.selectedFloor);
    break;
  }
}

void handleSimulationControls(SDL_Keycode key) {
  switch (key) {
  case SDLK_U:
    if (ui.selectedFloor < MAX_FLOOR) {
      simulation.spawnPassenger(
          ui.selectedFloor,
          randomDestination(ui.selectedFloor, Direction::Up));
    }
    break;

  case SDLK_D:
    if (ui.selectedFloor > MIN_FLOOR) {
      simulation.spawnPassenger(
          ui.selectedFloor,
          randomDestination(ui.selectedFloor, Direction::Down));
    }
    break;
  }
}

SDL_AppResult handleKey(SDL_Keycode key, SDL_Keymod modifiers) {
  const bool questionPressed =
      key == SDLK_QUESTION ||
      (key == SDLK_SLASH && (modifiers & SDL_KMOD_SHIFT) != 0);
  const bool modeMenuPressed =
      key == SDLK_SLASH && (modifiers & SDL_KMOD_SHIFT) == 0;

  if (key == SDLK_Q) {
    return SDL_APP_SUCCESS;
  }

  if (showModeMenu) {
    if (key == SDLK_1) {
      switchMode(AppMode::Manual);
    } else if (key == SDLK_2) {
      switchMode(AppMode::Simulation);
    } else if (key == SDLK_ESCAPE || modeMenuPressed) {
      showModeMenu = false;
      accumulatorSeconds = 0.0;
    }
    return SDL_APP_CONTINUE;
  }

  if (modeMenuPressed) {
    ui.showHelp = false;
    showModeMenu = true;
    accumulatorSeconds = 0.0;
    return SDL_APP_CONTINUE;
  }

  if (key == SDLK_H || questionPressed) {
    ui.showHelp = !ui.showHelp;
    return SDL_APP_CONTINUE;
  }

  if (ui.showHelp) {
    if (key == SDLK_ESCAPE) {
      ui.showHelp = false;
    }
    return SDL_APP_CONTINUE;
  }

  handleSharedControls(key);

  switch (activeMode) {
  case AppMode::Manual:
    handleManualControls(key);
    break;
  case AppMode::Simulation:
    handleSimulationControls(key);
    break;
  }

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppInit(void **, int, char *[]) {
  SDL_SetAppMetadata("Elevator Simulation", "1.0", nullptr);

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  if (!SDL_CreateWindowAndRenderer(
          "Elevator Simulation", rendering::WINDOW_WIDTH,
          rendering::WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE, &window,
          &renderer)) {
    SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  SDL_SetRenderLogicalPresentation(renderer, rendering::WINDOW_WIDTH,
                                   rendering::WINDOW_HEIGHT,
                                   SDL_LOGICAL_PRESENTATION_LETTERBOX);

  previousTimeNs = SDL_GetTicksNS();

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *, SDL_Event *event) {
  if (event->type == SDL_EVENT_QUIT) {
    return SDL_APP_SUCCESS;
  }

  if (event->type == SDL_EVENT_KEY_DOWN && !event->key.repeat) {
    return handleKey(event->key.key, event->key.mod);
  }

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *) {
  const Uint64 currentTimeNs = SDL_GetTicksNS();
  const double elapsedSeconds = std::min(
      static_cast<double>(currentTimeNs - previousTimeNs) /
          NANOSECONDS_PER_SECOND,
      MAX_FRAME_TIME_SECONDS);
  previousTimeNs = currentTimeNs;

  if (!ui.paused && !showModeMenu) {
    accumulatorSeconds += elapsedSeconds;
  }

  while (accumulatorSeconds >= simulation_timing::SECONDS_PER_TICK) {
    simulation.tick();
    accumulatorSeconds -= simulation_timing::SECONDS_PER_TICK;
  }

  rendering::renderFrame(renderer, ui, simulation.snapshot(),
                         activeMode == AppMode::Simulation, showModeMenu);
  return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *, SDL_AppResult) {}
