#include "rendering.hpp"

namespace {

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

constexpr float PANEL_X = BUILDING_X + BUILDING_WIDTH + 24.0F;
constexpr float PANEL_Y = BUILDING_Y;
constexpr float PANEL_WIDTH = rendering::WINDOW_WIDTH - PANEL_X - 40.0F;
constexpr float PANEL_HEIGHT = BUILDING_HEIGHT;

struct RenderContext {
  SDL_Renderer *renderer;
  const rendering::UiState &ui;
  const SimulationSnapshot &snapshot;
  float shaftWidth;
};

int rowForFloor(int floor) { return MAX_FLOOR - floor; }

float calculateShaftWidth(std::size_t elevatorCount) {
  return (SHAFT_AREA_WIDTH -
          SHAFT_GAP * (static_cast<float>(elevatorCount) + 1.0F)) /
         static_cast<float>(elevatorCount);
}

float shaftX(const RenderContext &context, ElevatorId elevatorId) {
  return SHAFT_AREA_X + SHAFT_GAP +
         static_cast<float>(elevatorId) *
             (context.shaftWidth + SHAFT_GAP);
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

void renderBuilding(const RenderContext &context) {
  SDL_Renderer *renderer = context.renderer;
  const rendering::UiState &uiState = context.ui;
  const SimulationSnapshot &snapshot = context.snapshot;

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
    SDL_RenderDebugTextFormat(renderer, INNER_X + 16.0F, floorY + 16.0F,
                              "FLOOR %d", floor);

    if (floor == uiState.selectedFloor) {
      SDL_SetRenderDrawColor(renderer, 78, 190, 220, 255);
      const SDL_FRect floorHighlight{INNER_X + 2.0F, floorY + 2.0F,
                                     WAITING_AREA_WIDTH - 4.0F,
                                     FLOOR_HEIGHT - 4.0F};
      SDL_RenderRect(renderer, &floorHighlight);
    }
  }

  SDL_SetRenderDrawColor(renderer, 9, 13, 18, 255);
  for (ElevatorId id = 0; id < snapshot.elevators.size(); ++id) {
    const SDL_FRect shaft{shaftX(context, id), INNER_Y, context.shaftWidth,
                          INNER_HEIGHT};
    SDL_RenderFillRect(renderer, &shaft);
  }

  SDL_SetRenderDrawColor(renderer, 78, 190, 220, 255);
  const SDL_FRect selectedShaft{
      shaftX(context, uiState.selectedElevator) - 2.0F, INNER_Y - 2.0F,
      context.shaftWidth + 4.0F, INNER_HEIGHT + 4.0F};
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

  for (ElevatorId id = 0; id < snapshot.elevators.size(); ++id) {
    if (id == uiState.selectedElevator) {
      SDL_SetRenderDrawColor(renderer, 78, 190, 220, 255);
    } else {
      SDL_SetRenderDrawColor(renderer, 210, 217, 224, 255);
    }
    SDL_RenderDebugTextFormat(
        renderer, shaftX(context, id) + context.shaftWidth / 2.0F - 8.0F,
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
          snapshot.assignedElevator({floor, Direction::Up}).has_value();
      SDL_SetRenderDrawColor(renderer, active ? 116 : 67, active ? 201 : 78,
                             active ? 142 : 90, 255);
      const SDL_FRect upButton{buttonX, upY, BUTTON_SIZE, BUTTON_SIZE};
      SDL_RenderFillRect(renderer, &upButton);

      SDL_SetRenderDrawColor(renderer, 225, 231, 236, 255);
      SDL_RenderDebugText(renderer, buttonX + 5.0F, upY + 5.0F, "U");
    }

    if (hasDownButton) {
      const bool active = snapshot
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

void renderElevator(const RenderContext &context, ElevatorId id,
                    const ElevatorSnapshot &snapshot, bool selected) {
  SDL_Renderer *renderer = context.renderer;
  const int row = rowForFloor(snapshot.currentFloor);
  const float floorY = INNER_Y + static_cast<float>(row) * FLOOR_HEIGHT;

  constexpr float CAR_MARGIN_X = 7.0F;
  constexpr float CAR_MARGIN_Y = 10.0F;

  const SDL_FRect car{shaftX(context, id) + CAR_MARGIN_X,
                      floorY + CAR_MARGIN_Y,
                      context.shaftWidth - 2.0F * CAR_MARGIN_X,
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

void renderElevators(const RenderContext &context) {
  for (ElevatorId id = 0; id < context.snapshot.elevators.size(); ++id) {
    renderElevator(context, id, context.snapshot.elevators[id],
                   id == context.ui.selectedElevator);
  }
}

void renderStatusPanel(const RenderContext &context) {
  SDL_Renderer *renderer = context.renderer;
  const rendering::UiState &uiState = context.ui;
  const SimulationSnapshot &snapshot = context.snapshot;

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
  SDL_RenderDebugTextFormat(renderer, PANEL_X + 18.0F, PANEL_Y + 86.0F,
                            "SIMULATION: %s",
                            uiState.paused ? "PAUSED" : "RUNNING");

  for (ElevatorId id = 0; id < snapshot.elevators.size(); ++id) {
    const ElevatorSnapshot &elevator = snapshot.elevators[id];
    const float y = PANEL_Y + 116.0F + static_cast<float>(id) * 54.0F;

    SDL_SetRenderDrawColor(renderer, 210, 217, 224, 255);
    SDL_RenderDebugTextFormat(renderer, PANEL_X + 18.0F, y, "E%zu  FLOOR %d",
                              id, elevator.currentFloor);
    SDL_RenderDebugTextFormat(
        renderer, PANEL_X + 18.0F, y + 18.0F, "%s  PENDING: %s",
        directionLabel(elevator.direction),
        elevator.hasPendingRequests ? "YES" : "NO");
  }

  const auto upAssignment =
      snapshot.assignedElevator({uiState.selectedFloor, Direction::Up});
  const auto downAssignment =
      snapshot.assignedElevator({uiState.selectedFloor, Direction::Down});

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
                      PANEL_Y + PANEL_HEIGHT - 78.0F,
                      "SPACE: PAUSE / RESUME");
  SDL_RenderDebugText(renderer, PANEL_X + 18.0F,
                      PANEL_Y + PANEL_HEIGHT - 60.0F,
                      "N: STEP WHEN PAUSED");
  SDL_RenderDebugText(renderer, PANEL_X + 18.0F,
                      PANEL_Y + PANEL_HEIGHT - 42.0F, "H / ?: HELP");
}

void renderHelpOverlay(SDL_Renderer *renderer) {
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, 4, 7, 10, 205);
  const SDL_FRect shade{0.0F, 0.0F,
                        static_cast<float>(rendering::WINDOW_WIDTH),
                        static_cast<float>(rendering::WINDOW_HEIGHT)};
  SDL_RenderFillRect(renderer, &shade);

  constexpr float HELP_WIDTH = 650.0F;
  constexpr float HELP_HEIGHT = 430.0F;
  constexpr float HELP_X = (rendering::WINDOW_WIDTH - HELP_WIDTH) / 2.0F;
  constexpr float HELP_Y = (rendering::WINDOW_HEIGHT - HELP_HEIGHT) / 2.0F;

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

  SDL_RenderDebugText(renderer, LABEL_X, y, "1 - 8");
  SDL_RenderDebugText(renderer, DESCRIPTION_X, y, "SELECT FLOOR");
  y += 38.0F;
  SDL_RenderDebugText(renderer, LABEL_X, y, "LEFT / RIGHT");
  SDL_RenderDebugText(renderer, DESCRIPTION_X, y, "SELECT ELEVATOR");
  y += 38.0F;
  SDL_RenderDebugText(renderer, LABEL_X, y, "U / D");
  SDL_RenderDebugText(renderer, DESCRIPTION_X, y, "ADD HALL CALL");
  y += 38.0F;
  SDL_RenderDebugText(renderer, LABEL_X, y, "E");
  SDL_RenderDebugText(renderer, DESCRIPTION_X, y, "ADD INTERNAL REQUEST");
  y += 38.0F;
  SDL_RenderDebugText(renderer, LABEL_X, y, "SPACE");
  SDL_RenderDebugText(renderer, DESCRIPTION_X, y, "PAUSE / RESUME");
  y += 38.0F;
  SDL_RenderDebugText(renderer, LABEL_X, y, "N");
  SDL_RenderDebugText(renderer, DESCRIPTION_X, y, "STEP WHILE PAUSED");
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

} // namespace

void rendering::renderFrame(SDL_Renderer *renderer, const UiState &uiState,
                            const SimulationSnapshot &snapshot) {
  const RenderContext context{renderer, uiState, snapshot,
                              calculateShaftWidth(snapshot.elevators.size())};

  SDL_SetRenderDrawColor(renderer, 16, 21, 29, 255);
  SDL_RenderClear(renderer);

  renderBuilding(context);
  renderElevators(context);
  renderStatusPanel(context);

  if (uiState.showHelp) {
    renderHelpOverlay(renderer);
  }

  SDL_RenderPresent(renderer);
}
