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

struct UiState {
  int selectedFloor = MIN_FLOOR;
  ElevatorId selectedElevator = 0;
  bool showHelp = false;
};

static UiState ui;

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

void renderBuilding(const UiState &uiState) {
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

    if (floor == uiState.selectedFloor) {
      SDL_SetRenderDrawColor(renderer, 78, 190, 220, 255);
    } else {
      SDL_SetRenderDrawColor(renderer, 210, 217, 224, 255);
    }
    SDL_RenderDebugTextFormat(renderer, INNER_X + 16.0F,
                              floorY + 16.0F, "FLOOR %d", floor);

    if (floor == uiState.selectedFloor) {
      SDL_SetRenderDrawColor(renderer, 78, 190, 220, 255);
      const SDL_FRect floorHighlight{INNER_X + 2.0F, floorY + 2.0F,
                                     WAITING_AREA_WIDTH - 4.0F,
                                     FLOOR_HEIGHT - 4.0F};
      SDL_RenderRect(renderer, &floorHighlight);
    }
  }

  SDL_SetRenderDrawColor(renderer, 9, 13, 18, 255);
  for (ElevatorId id = 0; id < dispatcher.elevatorCount(); ++id) {
    const SDL_FRect shaft{shaftX(id), INNER_Y, SHAFT_WIDTH, INNER_HEIGHT};
    SDL_RenderFillRect(renderer, &shaft);
  }

  SDL_SetRenderDrawColor(renderer, 78, 190, 220, 255);
  const SDL_FRect selectedShaft{
      shaftX(uiState.selectedElevator) - 2.0F, INNER_Y - 2.0F,
      SHAFT_WIDTH + 4.0F, INNER_HEIGHT + 4.0F};
  SDL_RenderRect(renderer, &selectedShaft);

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

  for (ElevatorId id = 0; id < dispatcher.elevatorCount(); ++id) {
    if (id == uiState.selectedElevator) {
      SDL_SetRenderDrawColor(renderer, 78, 190, 220, 255);
    } else {
      SDL_SetRenderDrawColor(renderer, 210, 217, 224, 255);
    }
    SDL_RenderDebugTextFormat(renderer, shaftX(id) + SHAFT_WIDTH / 2.0F - 8.0F,
                              BUILDING_Y + 10.0F, "E%zu", id);
  }

  constexpr float BUTTON_SIZE = 18.0F;
  constexpr float BUTTON_GAP = 7.0F;
  const float buttonX = SHAFT_AREA_X - BUTTON_SIZE - 18.0F;

  for (int floor = MIN_FLOOR; floor <= MAX_FLOOR; ++floor) {
    const int row = rowForFloor(floor);
    const float floorY = INNER_Y + static_cast<float>(row) * FLOOR_HEIGHT;
    const bool hasUpButton = floor < MAX_FLOOR;
    const bool hasDownButton = floor > MIN_FLOOR;
    const bool hasBothButtons = hasUpButton && hasDownButton;
    const float singleButtonY =
        floorY + (FLOOR_HEIGHT - BUTTON_SIZE) / 2.0F;
    const float upY =
        hasBothButtons
            ? floorY +
                  (FLOOR_HEIGHT - (2.0F * BUTTON_SIZE + BUTTON_GAP)) / 2.0F
            : singleButtonY;
    const float downY =
        hasBothButtons ? upY + BUTTON_SIZE + BUTTON_GAP : singleButtonY;

    if (hasUpButton) {
      const bool active =
          dispatcher.assignedElevator({floor, Direction::Up}).has_value();
      SDL_SetRenderDrawColor(renderer, active ? 116 : 67, active ? 201 : 78,
                             active ? 142 : 90, 255);
      const SDL_FRect upButton{buttonX, upY, BUTTON_SIZE, BUTTON_SIZE};
      SDL_RenderFillRect(renderer, &upButton);

      SDL_SetRenderDrawColor(renderer, 225, 231, 236, 255);
      SDL_RenderDebugText(renderer, buttonX + 5.0F, upY + 5.0F, "U");
    }

    if (hasDownButton) {
      const bool active = dispatcher
                              .assignedElevator({floor, Direction::Down})
                              .has_value();
      SDL_SetRenderDrawColor(renderer, active ? 220 : 67, active ? 112 : 78,
                             active ? 112 : 90, 255);
      const SDL_FRect downButton{buttonX, downY, BUTTON_SIZE, BUTTON_SIZE};
      SDL_RenderFillRect(renderer, &downButton);

      SDL_SetRenderDrawColor(renderer, 225, 231, 236, 255);
      SDL_RenderDebugText(renderer, buttonX + 5.0F, downY + 5.0F, "D");
    }
  }
}

void renderElevator(ElevatorId id, const ElevatorSnapshot &snapshot,
                    bool selected) {
  const int row = rowForFloor(snapshot.currentFloor);
  const float floorY = INNER_Y + static_cast<float>(row) * FLOOR_HEIGHT;

  constexpr float CAR_MARGIN_X = 7.0F;
  constexpr float CAR_MARGIN_Y = 10.0F;

  const SDL_FRect car{shaftX(id) + CAR_MARGIN_X, floorY + CAR_MARGIN_Y,
                      SHAFT_WIDTH - 2.0F * CAR_MARGIN_X,
                      FLOOR_HEIGHT - 2.0F * CAR_MARGIN_Y};

  if (selected) {
    SDL_SetRenderDrawColor(renderer, 78, 190, 220, 255);
    const SDL_FRect highlight{car.x - 4.0F, car.y - 4.0F, car.w + 8.0F,
                              car.h + 8.0F};
    SDL_RenderFillRect(renderer, &highlight);
  }

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

void renderElevators(const UiState &uiState) {
  for (ElevatorId id = 0; id < dispatcher.elevatorCount(); ++id) {
    renderElevator(id, dispatcher.elevatorSnapshot(id),
                   id == uiState.selectedElevator);
  }
}

void renderStatusPanel(const UiState &uiState) {
  SDL_SetRenderDrawColor(renderer, 25, 33, 43, 255);
  const SDL_FRect panel{PANEL_X, PANEL_Y, PANEL_WIDTH, PANEL_HEIGHT};
  SDL_RenderFillRect(renderer, &panel);

  SDL_SetRenderDrawColor(renderer, 210, 217, 224, 255);
  SDL_RenderDebugText(renderer, PANEL_X + 18.0F, PANEL_Y + 20.0F,
                      "ELEVATOR CONTROL");

  SDL_SetRenderDrawColor(renderer, 78, 190, 220, 255);
  SDL_RenderDebugTextFormat(renderer, PANEL_X + 18.0F, PANEL_Y + 50.0F,
                            "SELECTED FLOOR: %d", uiState.selectedFloor);
  SDL_RenderDebugTextFormat(renderer, PANEL_X + 18.0F, PANEL_Y + 68.0F,
                            "SELECTED ELEVATOR: E%zu",
                            uiState.selectedElevator);

  for (ElevatorId id = 0; id < dispatcher.elevatorCount(); ++id) {
    const ElevatorSnapshot snapshot = dispatcher.elevatorSnapshot(id);
    const float y = PANEL_Y + 116.0F + static_cast<float>(id) * 54.0F;

    SDL_SetRenderDrawColor(renderer, 210, 217, 224, 255);
    SDL_RenderDebugTextFormat(renderer, PANEL_X + 18.0F, y,
                              "E%zu  FLOOR %d", id, snapshot.currentFloor);
    SDL_RenderDebugTextFormat(
        renderer, PANEL_X + 18.0F, y + 18.0F, "%s  PENDING: %s",
        directionLabel(snapshot.direction),
        snapshot.hasPendingRequests ? "YES" : "NO");
  }

  const auto upAssignment =
      dispatcher.assignedElevator({uiState.selectedFloor, Direction::Up});
  const auto downAssignment =
      dispatcher.assignedElevator({uiState.selectedFloor, Direction::Down});

  SDL_SetRenderDrawColor(renderer, 210, 217, 224, 255);
  const float assignmentY = PANEL_Y + 300.0F;
  if (uiState.selectedFloor < MAX_FLOOR) {
    if (upAssignment) {
      SDL_RenderDebugTextFormat(renderer, PANEL_X + 18.0F, assignmentY,
                                "UP CALL: E%zu", *upAssignment);
    } else {
      SDL_RenderDebugText(renderer, PANEL_X + 18.0F, assignmentY,
                          "UP CALL: --");
    }
  }

  if (uiState.selectedFloor > MIN_FLOOR) {
    if (downAssignment) {
      SDL_RenderDebugTextFormat(renderer, PANEL_X + 18.0F,
                                assignmentY + 18.0F, "DOWN CALL: E%zu",
                                *downAssignment);
    } else {
      SDL_RenderDebugText(renderer, PANEL_X + 18.0F, assignmentY + 18.0F,
                          "DOWN CALL: --");
    }
  }

  SDL_SetRenderDrawColor(renderer, 115, 128, 142, 255);
  SDL_RenderDebugText(renderer, PANEL_X + 18.0F,
                      PANEL_Y + PANEL_HEIGHT - 60.0F, "SPACE: ADVANCE");
  SDL_RenderDebugText(renderer, PANEL_X + 18.0F,
                      PANEL_Y + PANEL_HEIGHT - 42.0F, "H / ?: HELP");
}

void renderHelpOverlay() {
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, 4, 7, 10, 205);
  const SDL_FRect shade{0.0F, 0.0F, static_cast<float>(WIN_WIDTH),
                        static_cast<float>(WIN_HEIGHT)};
  SDL_RenderFillRect(renderer, &shade);

  constexpr float HELP_WIDTH = 650.0F;
  constexpr float HELP_HEIGHT = 430.0F;
  constexpr float HELP_X = (WIN_WIDTH - HELP_WIDTH) / 2.0F;
  constexpr float HELP_Y = (WIN_HEIGHT - HELP_HEIGHT) / 2.0F;

  SDL_SetRenderDrawColor(renderer, 31, 41, 53, 255);
  const SDL_FRect helpPanel{HELP_X, HELP_Y, HELP_WIDTH, HELP_HEIGHT};
  SDL_RenderFillRect(renderer, &helpPanel);

  SDL_SetRenderDrawColor(renderer, 78, 190, 220, 255);
  SDL_RenderRect(renderer, &helpPanel);
  SDL_RenderDebugText(renderer, HELP_X + 30.0F, HELP_Y + 26.0F,
                      "KEYBOARD CONTROLS");

  SDL_SetRenderDrawColor(renderer, 225, 231, 236, 255);
  constexpr float LABEL_X = HELP_X + 38.0F;
  constexpr float DESCRIPTION_X = HELP_X + 210.0F;
  float y = HELP_Y + 78.0F;

  SDL_RenderDebugText(renderer, LABEL_X, y, "1 - 5");
  SDL_RenderDebugText(renderer, DESCRIPTION_X, y, "SELECT FLOOR");
  y += 38.0F;
  SDL_RenderDebugText(renderer, LABEL_X, y, "LEFT / RIGHT");
  SDL_RenderDebugText(renderer, DESCRIPTION_X, y, "SELECT ELEVATOR");
  y += 38.0F;
  SDL_RenderDebugText(renderer, LABEL_X, y, "U / D");
  SDL_RenderDebugText(renderer, DESCRIPTION_X, y, "ADD HALL CALL");
  y += 38.0F;
  SDL_RenderDebugText(renderer, LABEL_X, y, "E");
  SDL_RenderDebugText(renderer, DESCRIPTION_X, y,
                      "ADD INTERNAL REQUEST");
  y += 38.0F;
  SDL_RenderDebugText(renderer, LABEL_X, y, "SPACE");
  SDL_RenderDebugText(renderer, DESCRIPTION_X, y, "ADVANCE ONE STEP");
  y += 38.0F;
  SDL_RenderDebugText(renderer, LABEL_X, y, "H / ?");
  SDL_RenderDebugText(renderer, DESCRIPTION_X, y, "TOGGLE HELP");
  y += 38.0F;
  SDL_RenderDebugText(renderer, LABEL_X, y, "Q");
  SDL_RenderDebugText(renderer, DESCRIPTION_X, y, "QUIT");

  SDL_SetRenderDrawColor(renderer, 115, 128, 142, 255);
  SDL_RenderDebugText(renderer, HELP_X + 30.0F,
                      HELP_Y + HELP_HEIGHT - 34.0F,
                      "H, ?, OR ESCAPE TO CLOSE");

  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

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
    ui.selectedFloor =
        static_cast<int>(key - SDLK_1) + MIN_FLOOR;
    return SDL_APP_CONTINUE;
  }

  switch (key) {
  case SDLK_LEFT:
    if (ui.selectedElevator > 0) {
      --ui.selectedElevator;
    }
    break;

  case SDLK_RIGHT:
    if (ui.selectedElevator + 1 < dispatcher.elevatorCount()) {
      ++ui.selectedElevator;
    }
    break;

  case SDLK_U:
    if (ui.selectedFloor < MAX_FLOOR) {
      dispatcher.addHallCall({ui.selectedFloor, Direction::Up});
    }
    break;

  case SDLK_D:
    if (ui.selectedFloor > MIN_FLOOR) {
      dispatcher.addHallCall({ui.selectedFloor, Direction::Down});
    }
    break;

  case SDLK_E:
    dispatcher.selectFloor(ui.selectedElevator, ui.selectedFloor);
    break;

  case SDLK_SPACE:
    dispatcher.advance();
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

  if (event->type == SDL_EVENT_KEY_DOWN && !event->key.repeat) {
    return handleKey(event->key.key, event->key.mod);
  }

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *) {
  SDL_SetRenderDrawColor(renderer, 16, 21, 29, 255);
  SDL_RenderClear(renderer);

  renderBuilding(ui);
  renderElevators(ui);
  renderStatusPanel(ui);

  if (ui.showHelp) {
    renderHelpOverlay();
  }

  SDL_RenderPresent(renderer);
  return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *, SDL_AppResult) {}
