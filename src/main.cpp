#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "dispatcher.hpp"

constexpr int NUM_ELEVATORS = 3;
constexpr int WIN_WIDTH = 1280;
constexpr int WIN_HEIGHT = 720;

constexpr float BUILDING_X = 40.0F;
constexpr float BUILDING_Y = 30.0F;
constexpr float BUILDING_WIDTH = 900.0F;
constexpr float BUILDING_HEIGHT = 660.0F;
constexpr float WALL_WIDTH = 8.0F;
constexpr float WAITING_AREA_WIDTH = 400.0F;
constexpr float SHAFT_GAP = 12.0F;
constexpr float FLOOR_SLAB_HEIGHT = 6.0F;

constexpr float INNER_X = BUILDING_X + WALL_WIDTH;
constexpr float INNER_Y = BUILDING_Y + WALL_WIDTH;
constexpr float INNER_WIDTH = BUILDING_WIDTH - 2.0F * WALL_WIDTH;
constexpr float INNER_HEIGHT = BUILDING_HEIGHT - 2.0F * WALL_WIDTH;
constexpr float FLOOR_HEIGHT = INNER_HEIGHT / MAX_FLOOR;
constexpr float SHAFT_AREA_X = INNER_X + WAITING_AREA_WIDTH;
constexpr float SHAFT_AREA_WIDTH = INNER_WIDTH - WAITING_AREA_WIDTH;
constexpr float SHAFT_WIDTH =
    (SHAFT_AREA_WIDTH - SHAFT_GAP * (NUM_ELEVATORS + 1)) / NUM_ELEVATORS;

constexpr float PANEL_X = BUILDING_X + BUILDING_WIDTH + 24.0F;
constexpr float PANEL_Y = BUILDING_Y;
constexpr float PANEL_WIDTH = WIN_WIDTH - PANEL_X - 40.0F;
constexpr float PANEL_HEIGHT = BUILDING_HEIGHT;

static SDL_Window *window = nullptr;
static SDL_Renderer *renderer = nullptr;
static Dispatcher dispatcher{NUM_ELEVATORS};

int rowForFloor(int floor) { return MAX_FLOOR - floor; }

float shaftX(ElevatorId elevatorId) {
  return SHAFT_AREA_X + SHAFT_GAP +
         static_cast<float>(elevatorId) * (SHAFT_WIDTH + SHAFT_GAP);
}

const char *directionLabel(Direction direction) {
  switch (direction) {
  case Direction::Up:
    return "UP";
  case Direction::Down:
    return "DOWN";
  case Direction::Idle:
    return "IDLE";
  }

  return "UNKNOWN";
}

void renderBuilding() {
  SDL_SetRenderDrawColor(renderer, 48, 59, 71, 255);
  const SDL_FRect building{BUILDING_X, BUILDING_Y, BUILDING_WIDTH,
                           BUILDING_HEIGHT};
  SDL_RenderFillRect(renderer, &building);

  for (int floor = MIN_FLOOR; floor <= MAX_FLOOR; ++floor) {
    const int row = rowForFloor(floor);
    const float floorY = INNER_Y + static_cast<float>(row) * FLOOR_HEIGHT;

    const Uint8 shade = row % 2 == 0 ? 31 : 35;
    SDL_SetRenderDrawColor(renderer, shade, shade + 9, shade + 19, 255);
    const SDL_FRect waitingArea{INNER_X, floorY, WAITING_AREA_WIDTH,
                                FLOOR_HEIGHT};
    SDL_RenderFillRect(renderer, &waitingArea);

    SDL_SetRenderDrawColor(renderer, 210, 217, 224, 255);
    SDL_RenderDebugTextFormat(renderer, INNER_X + 16.0F,
                              floorY + 16.0F, "FLOOR %d", floor);
  }

  SDL_SetRenderDrawColor(renderer, 9, 13, 18, 255);
  for (ElevatorId id = 0; id < dispatcher.elevatorCount(); ++id) {
    const SDL_FRect shaft{shaftX(id), INNER_Y, SHAFT_WIDTH, INNER_HEIGHT};
    SDL_RenderFillRect(renderer, &shaft);
  }

  for (int row = 1; row < MAX_FLOOR; ++row) {
    const float boundaryY = INNER_Y + static_cast<float>(row) * FLOOR_HEIGHT;

    SDL_SetRenderDrawColor(renderer, 105, 118, 132, 255);
    const SDL_FRect slab{INNER_X, boundaryY - FLOOR_SLAB_HEIGHT / 2.0F,
                         WAITING_AREA_WIDTH, FLOOR_SLAB_HEIGHT};
    SDL_RenderFillRect(renderer, &slab);

    SDL_SetRenderDrawColor(renderer, 42, 51, 61, 255);
    const SDL_FRect shaftGuide{SHAFT_AREA_X, boundaryY - 1.0F,
                               SHAFT_AREA_WIDTH, 2.0F};
    SDL_RenderFillRect(renderer, &shaftGuide);
  }

  SDL_SetRenderDrawColor(renderer, 105, 118, 132, 255);
  const SDL_FRect lobbyFloor{INNER_X, INNER_Y + INNER_HEIGHT - 6.0F,
                             WAITING_AREA_WIDTH, 6.0F};
  SDL_RenderFillRect(renderer, &lobbyFloor);

  SDL_SetRenderDrawColor(renderer, 210, 217, 224, 255);
  for (ElevatorId id = 0; id < dispatcher.elevatorCount(); ++id) {
    SDL_RenderDebugTextFormat(renderer, shaftX(id) + SHAFT_WIDTH / 2.0F - 8.0F,
                              BUILDING_Y + 10.0F, "E%zu", id);
  }
}

void renderElevator(ElevatorId id, const ElevatorSnapshot &snapshot) {
  const int row = rowForFloor(snapshot.currentFloor);
  const float floorY = INNER_Y + static_cast<float>(row) * FLOOR_HEIGHT;

  constexpr float CAR_MARGIN_X = 7.0F;
  constexpr float CAR_MARGIN_Y = 10.0F;

  const SDL_FRect car{shaftX(id) + CAR_MARGIN_X, floorY + CAR_MARGIN_Y,
                      SHAFT_WIDTH - 2.0F * CAR_MARGIN_X,
                      FLOOR_HEIGHT - 2.0F * CAR_MARGIN_Y};

  SDL_SetRenderDrawColor(renderer, 216, 168, 78, 255);
  SDL_RenderFillRect(renderer, &car);

  constexpr float CAR_BORDER = 5.0F;
  const SDL_FRect doors{car.x + CAR_BORDER, car.y + CAR_BORDER,
                        car.w - 2.0F * CAR_BORDER,
                        car.h - 2.0F * CAR_BORDER};
  SDL_SetRenderDrawColor(renderer, 183, 194, 204, 255);
  SDL_RenderFillRect(renderer, &doors);

  SDL_SetRenderDrawColor(renderer, 54, 64, 74, 255);
  const SDL_FRect doorSeam{doors.x + doors.w / 2.0F - 1.0F, doors.y, 2.0F,
                           doors.h};
  SDL_RenderFillRect(renderer, &doorSeam);

  if (snapshot.direction == Direction::Up) {
    SDL_SetRenderDrawColor(renderer, 116, 201, 142, 255);
  } else if (snapshot.direction == Direction::Down) {
    SDL_SetRenderDrawColor(renderer, 220, 112, 112, 255);
  } else {
    SDL_SetRenderDrawColor(renderer, 115, 128, 142, 255);
  }
  const SDL_FRect indicator{car.x + car.w / 2.0F - 12.0F, car.y + 7.0F,
                            24.0F, 5.0F};
  SDL_RenderFillRect(renderer, &indicator);
}

void renderElevators() {
  for (ElevatorId id = 0; id < dispatcher.elevatorCount(); ++id) {
    renderElevator(id, dispatcher.elevatorSnapshot(id));
  }
}

void renderStatusPanel() {
  SDL_SetRenderDrawColor(renderer, 25, 33, 43, 255);
  const SDL_FRect panel{PANEL_X, PANEL_Y, PANEL_WIDTH, PANEL_HEIGHT};
  SDL_RenderFillRect(renderer, &panel);

  SDL_SetRenderDrawColor(renderer, 210, 217, 224, 255);
  SDL_RenderDebugText(renderer, PANEL_X + 18.0F, PANEL_Y + 20.0F,
                      "ELEVATOR STATUS");

  for (ElevatorId id = 0; id < dispatcher.elevatorCount(); ++id) {
    const ElevatorSnapshot snapshot = dispatcher.elevatorSnapshot(id);
    const float y = PANEL_Y + 60.0F + static_cast<float>(id) * 54.0F;

    SDL_RenderDebugTextFormat(renderer, PANEL_X + 18.0F, y,
                              "E%zu  FLOOR %d", id, snapshot.currentFloor);
    SDL_RenderDebugTextFormat(renderer, PANEL_X + 18.0F, y + 18.0F, "%s",
                              directionLabel(snapshot.direction));
  }

  SDL_SetRenderDrawColor(renderer, 115, 128, 142, 255);
  SDL_RenderDebugText(renderer, PANEL_X + 18.0F,
                      PANEL_Y + PANEL_HEIGHT - 42.0F,
                      "STATIC CORE SNAPSHOT");
}

SDL_AppResult SDL_AppInit(void **, int, char *[]) {
  SDL_SetAppMetadata("Elevator Simulation", "1.0", nullptr);

  if (!SDL_Init(SDL_INIT_VIDEO)) {
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

SDL_AppResult SDL_AppEvent(void *, SDL_Event *event) {
  if (event->type == SDL_EVENT_QUIT) {
    return SDL_APP_SUCCESS;
  }

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *) {
  SDL_SetRenderDrawColor(renderer, 16, 21, 29, 255);
  SDL_RenderClear(renderer);

  renderBuilding();
  renderElevators();
  renderStatusPanel();

  SDL_RenderPresent(renderer);
  return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *, SDL_AppResult) {}
