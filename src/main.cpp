#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "dispatcher.hpp"

constexpr int NUM_ELEVATORS = 3;

constexpr int WIN_WIDTH = 1280;
constexpr int WIN_HEIGHT = 720;

// -- Function Declarations --
void renderUI();

// -- Main Callback --
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static Dispatcher dispatcher{NUM_ELEVATORS};

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
  SDL_SetAppMetadata("Elevator Simulation", "1.0", NULL);

  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
    SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  if (!SDL_CreateWindowAndRenderer("Elevator Simulation", WIN_WIDTH, WIN_HEIGHT,
                                   SDL_WINDOW_RESIZABLE, &window, &renderer)) {
    SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }
  SDL_SetRenderLogicalPresentation(renderer, WIN_WIDTH, WIN_HEIGHT,
                                   SDL_LOGICAL_PRESENTATION_LETTERBOX);

  return SDL_APP_CONTINUE;
}

/*
function to handle events
user input logic goes here
*/
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
  switch (event->type) {
  case (SDL_EVENT_QUIT):
    return SDL_APP_SUCCESS;

  case (SDL_EVENT_MOUSE_BUTTON_DOWN):
    SDL_Log("Mouse button pressed at: %f, %f", event->button.x,
            event->button.y);
    break;
  }
  return SDL_APP_CONTINUE;
}

/*
function runs once per frame (the "main" code)
elevator logic goes here
*/
SDL_AppResult SDL_AppIterate(void *appstate) {
  SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
  SDL_RenderClear(renderer);

  // Building
  constexpr float BUILDING_WIDTH = 500;
  constexpr float BUILDING_HEIGHT = 695;

  SDL_SetRenderDrawColor(renderer, 0, 0, 0xFF, 0xFF);
  SDL_FRect building = {45, 25, BUILDING_WIDTH, BUILDING_HEIGHT};
  SDL_RenderFillRect(renderer, &building);

  // Floors
  constexpr float MARGIN = 10;
  constexpr float FLOOR_GAP = 10;
  constexpr float FLOOR_HEIGHT =
      (BUILDING_HEIGHT - MARGIN - FLOOR_GAP * (MAX_FLOOR - 1)) / MAX_FLOOR;
  constexpr float FLOOR_WIDTH = BUILDING_WIDTH - MARGIN * 2;

  SDL_SetRenderDrawColor(renderer, 0xFF, 0, 0, 0xFF);
  SDL_FRect floors[MAX_FLOOR];
  for (int i = 0; i < MAX_FLOOR; i++) {
    floors[i].x = building.x + MARGIN;
    floors[i].y = building.y + MARGIN + i * (FLOOR_HEIGHT + FLOOR_GAP);
    floors[i].w = FLOOR_WIDTH;
    floors[i].h = FLOOR_HEIGHT;
    SDL_RenderFillRect(renderer, &floors[i]);
  }

  // Elevator halls
  constexpr float HALL_GAP = 70;
  constexpr float HALL_WIDTH = 80;
  constexpr float HALL_HEIGHT = FLOOR_HEIGHT * 0.7;

  SDL_SetRenderDrawColor(renderer, 0, 0xFF, 0, 0xFF);
  SDL_FRect halls[MAX_FLOOR][NUM_ELEVATORS];
  for (int i = 0; i < MAX_FLOOR; i++) {
    for (int j = 0; j < NUM_ELEVATORS; j++) {
      halls[i][j].x = floors[i].x + MARGIN * 4 + j * (HALL_GAP + HALL_WIDTH);
      halls[i][j].y = floors[i].y + (FLOOR_HEIGHT - HALL_HEIGHT);
      halls[i][j].w = HALL_WIDTH;
      halls[i][j].h = HALL_HEIGHT;
      SDL_RenderFillRect(renderer, &halls[i][j]);
    }
  }

  // Hall Indicator
  constexpr float INDICATOR_WIDTH = HALL_WIDTH * 0.5;
  constexpr float INDICATOR_HEIGHT = HALL_HEIGHT * 0.2;
  constexpr float INDICATOR_GAP = 5;

  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0xFF);
  SDL_FRect indicators[NUM_ELEVATORS];
  for (int i = 0; i < NUM_ELEVATORS; i++) {
    SDL_FRect hall = halls[MAX_FLOOR - 1][i];
    indicators[i].x = hall.x + (hall.w - INDICATOR_WIDTH) * 0.5;
    indicators[i].y = hall.y - INDICATOR_GAP - INDICATOR_HEIGHT;
    indicators[i].w = INDICATOR_WIDTH;
    indicators[i].h = INDICATOR_HEIGHT;
    SDL_RenderFillRect(renderer, &indicators[i]);
  }

  SDL_RenderPresent(renderer);

  return SDL_APP_CONTINUE;
}

/*
function runs at shutdown
*/
void SDL_AppQuit(void *appstate, SDL_AppResult result) {
  /* SDL clean up automatically */
}
