#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "rendering.hpp"
#include "simulation.hpp"

constexpr int NUM_ELEVATORS = 3;

static SDL_Window *window = nullptr;
static SDL_Renderer *renderer = nullptr;
static Simulation simulation{NUM_ELEVATORS};
static rendering::UiState ui;

SDL_AppResult handleKey(SDL_Keycode key, SDL_Keymod modifiers) {
  const bool questionPressed =
      key == SDLK_QUESTION ||
      (key == SDLK_SLASH && (modifiers & SDL_KMOD_SHIFT) != 0);

  if (key == SDLK_H || questionPressed) {
    ui.showHelp = !ui.showHelp;
    return SDL_APP_CONTINUE;
  }

  if (key == SDLK_Q) {
    return SDL_APP_SUCCESS;
  }

  if (ui.showHelp) {
    if (key == SDLK_ESCAPE) {
      ui.showHelp = false;
    }
    return SDL_APP_CONTINUE;
  }

  if (key >= SDLK_1 && key <= SDLK_5) {
    ui.selectedFloor = static_cast<int>(key - SDLK_1) + MIN_FLOOR;
    return SDL_APP_CONTINUE;
  }

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

  case SDLK_SPACE:
    simulation.tick();
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
  rendering::renderFrame(renderer, ui, simulation.snapshot());
  return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *, SDL_AppResult) {}
