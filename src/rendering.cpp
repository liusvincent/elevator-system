#include "rendering.hpp"

#include <algorithm>
#include <cmath>

namespace {

constexpr float BUILDING_X = 40.0F;
constexpr float BUILDING_Y = 30.0F;
constexpr float BUILDING_WIDTH = 900.0F;
constexpr float BUILDING_HEIGHT = 660.0F;
constexpr float WALL_WIDTH = 8.0F;
constexpr float WAITING_AREA_WIDTH = 400.0F;
constexpr float SHAFT_GAP = 12.0F;
constexpr float FLOOR_SLAB_HEIGHT = 6.0F;
constexpr float CAR_MARGIN_X = 7.0F;
constexpr float CAR_MARGIN_Y = 10.0F;

constexpr SDL_Color UP_PASSENGER_COLOR{116, 201, 142, 255};
constexpr SDL_Color DOWN_PASSENGER_COLOR{220, 112, 112, 255};
constexpr SDL_Color RIDING_PASSENGER_COLOR{54, 64, 74, 255};

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
  bool simulationMode;
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

SDL_FRect elevatorCarRect(const RenderContext &context, ElevatorId id,
                          int floor) {
  const int row = rowForFloor(floor);
  const float floorY = INNER_Y + static_cast<float>(row) * FLOOR_HEIGHT;

  return {shaftX(context, id) + CAR_MARGIN_X, floorY + CAR_MARGIN_Y,
          context.shaftWidth - 2.0F * CAR_MARGIN_X,
          FLOOR_HEIGHT - 2.0F * CAR_MARGIN_Y};
}

void renderFilledCircle(SDL_Renderer *renderer, float centerX, float centerY,
                        float radius, SDL_Color color) {
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

  const int roundedRadius = static_cast<int>(std::ceil(radius));
  for (int y = -roundedRadius; y <= roundedRadius; ++y) {
    const float offsetY = static_cast<float>(y);
    const float width =
        std::sqrt(std::max(0.0F, radius * radius - offsetY * offsetY));
    SDL_RenderLine(renderer, centerX - width, centerY + offsetY,
                   centerX + width, centerY + offsetY);
  }
}

void renderPerson(SDL_Renderer *renderer, float centerX, float topY,
                  SDL_Color color, int destinationFloor,
                  float scale = 1.0F) {
  constexpr float HEAD_RADIUS = 3.0F;
  constexpr float BODY_TOP = 7.0F;
  constexpr float BODY_HALF_WIDTH = 4.0F;
  constexpr float BODY_BOTTOM = 15.0F;

  renderFilledCircle(renderer, centerX, topY + HEAD_RADIUS * scale,
                     HEAD_RADIUS * scale, color);

  const SDL_FColor vertexColor{
      static_cast<float>(color.r) / 255.0F,
      static_cast<float>(color.g) / 255.0F,
      static_cast<float>(color.b) / 255.0F,
      static_cast<float>(color.a) / 255.0F,
  };
  const SDL_Vertex body[] = {
      {{centerX - BODY_HALF_WIDTH * scale, topY + BODY_TOP * scale},
       vertexColor,
       {}},
      {{centerX + BODY_HALF_WIDTH * scale, topY + BODY_TOP * scale},
       vertexColor,
       {}},
      {{centerX, topY + BODY_BOTTOM * scale}, vertexColor, {}},
  };
  SDL_RenderGeometry(renderer, nullptr, body, 3, nullptr, 0);

  SDL_SetRenderDrawColor(renderer, 225, 231, 236, 255);
  SDL_RenderDebugTextFormat(renderer, centerX + 6.0F * scale,
                            topY + 5.0F * scale, "%d", destinationFloor);
}

Direction passengerDirection(const PassengerSnapshot &passenger) {
  return passenger.destinationFloor > passenger.originFloor
             ? Direction::Up
             : Direction::Down;
}

std::size_t elevatorOccupancy(const SimulationSnapshot &snapshot,
                              ElevatorId elevatorId) {
  std::size_t occupancy = 0;
  for (const PassengerSnapshot &passenger : snapshot.passengers) {
    if (passenger.elevatorId == elevatorId) {
      ++occupancy;
    }
  }
  return occupancy;
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

const char *modeLabel(bool simulationMode) {
  return simulationMode ? "SIMULATION" : "MANUAL";
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

  if (!context.simulationMode) {
    SDL_SetRenderDrawColor(renderer, 78, 190, 220, 255);
    const SDL_FRect selectedShaft{
        shaftX(context, uiState.selectedElevator) - 2.0F, INNER_Y - 2.0F,
        context.shaftWidth + 4.0F, INNER_HEIGHT + 4.0F};
    SDL_RenderRect(renderer, &selectedShaft);
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

  for (ElevatorId id = 0; id < snapshot.elevators.size(); ++id) {
    if (!context.simulationMode && id == uiState.selectedElevator) {
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
  const SDL_FRect car =
      elevatorCarRect(context, id, snapshot.currentFloor);

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
                   !context.simulationMode &&
                       id == context.ui.selectedElevator);
  }
}

void renderWaitingGroup(const RenderContext &context, int floor,
                        Direction direction, float groupX,
                        SDL_Color color) {
  constexpr std::size_t MAX_VISIBLE_PASSENGERS = 4;
  constexpr float PASSENGER_SPACING = 18.0F;

  const float floorY =
      INNER_Y + static_cast<float>(rowForFloor(floor)) * FLOOR_HEIGHT;
  const float passengerY = floorY + FLOOR_HEIGHT - 24.0F;
  std::size_t count = 0;

  for (const PassengerSnapshot &passenger : context.snapshot.passengers) {
    if (passenger.state != PassengerState::Waiting ||
        passenger.originFloor != floor ||
        passengerDirection(passenger) != direction) {
      continue;
    }

    if (count < MAX_VISIBLE_PASSENGERS) {
      renderPerson(context.renderer,
                   groupX + static_cast<float>(count) * PASSENGER_SPACING,
                   passengerY, color, passenger.destinationFloor);
    }
    ++count;
  }

  if (count > MAX_VISIBLE_PASSENGERS) {
    SDL_SetRenderDrawColor(context.renderer, color.r, color.g, color.b,
                           color.a);
    SDL_RenderDebugTextFormat(
        context.renderer,
        groupX + static_cast<float>(MAX_VISIBLE_PASSENGERS) *
                     PASSENGER_SPACING,
        passengerY + 4.0F, "+%zu", count - MAX_VISIBLE_PASSENGERS);
  }
}

void renderWaitingPassengers(const RenderContext &context) {
  constexpr float UP_GROUP_X = INNER_X + 130.0F;
  constexpr float DOWN_GROUP_X = INNER_X + 265.0F;

  for (int floor = MIN_FLOOR; floor <= MAX_FLOOR; ++floor) {
    renderWaitingGroup(context, floor, Direction::Up, UP_GROUP_X,
                       UP_PASSENGER_COLOR);
    renderWaitingGroup(context, floor, Direction::Down, DOWN_GROUP_X,
                       DOWN_PASSENGER_COLOR);
  }
}

void renderRidingPassengers(const RenderContext &context) {
  constexpr std::size_t PASSENGERS_PER_ROW = 3;
  constexpr float PERSON_SCALE = 0.75F;
  constexpr float COLUMN_SPACING = 18.0F;
  constexpr float ROW_SPACING = 16.0F;

  for (ElevatorId id = 0; id < context.snapshot.elevators.size(); ++id) {
    const ElevatorSnapshot &elevator = context.snapshot.elevators[id];
    const SDL_FRect car = elevatorCarRect(context, id, elevator.currentFloor);
    const float firstX = car.x + car.w / 2.0F - COLUMN_SPACING;
    const float firstY = car.y + 18.0F;
    std::size_t index = 0;

    for (const PassengerSnapshot &passenger : context.snapshot.passengers) {
      if (passenger.elevatorId != id) {
        continue;
      }

      const std::size_t column = index % PASSENGERS_PER_ROW;
      const std::size_t row = index / PASSENGERS_PER_ROW;
      renderPerson(context.renderer,
                   firstX + static_cast<float>(column) * COLUMN_SPACING,
                   firstY + static_cast<float>(row) * ROW_SPACING,
                   RIDING_PASSENGER_COLOR, passenger.destinationFloor,
                   PERSON_SCALE);
      ++index;
    }
  }
}

void renderPassengers(const RenderContext &context) {
  renderWaitingPassengers(context);
  renderRidingPassengers(context);
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
                            "MODE: %s",
                            modeLabel(context.simulationMode));
  SDL_RenderDebugTextFormat(renderer, PANEL_X + 18.0F, PANEL_Y + 68.0F,
                            context.simulationMode ? "ORIGIN FLOOR: %d"
                                                   : "SELECTED FLOOR: %d",
                            uiState.selectedFloor);

  float simulationStatusY = PANEL_Y + 86.0F;
  if (!context.simulationMode) {
    SDL_RenderDebugTextFormat(renderer, PANEL_X + 18.0F, PANEL_Y + 86.0F,
                              "SELECTED ELEVATOR: E%zu",
                              uiState.selectedElevator);
    simulationStatusY = PANEL_Y + 104.0F;
  }

  SDL_RenderDebugTextFormat(renderer, PANEL_X + 18.0F, simulationStatusY,
                            "SIMULATION: %s",
                            uiState.paused ? "PAUSED" : "RUNNING");

  const float elevatorStatusY =
      context.simulationMode ? PANEL_Y + 116.0F : PANEL_Y + 134.0F;
  for (ElevatorId id = 0; id < snapshot.elevators.size(); ++id) {
    const ElevatorSnapshot &elevator = snapshot.elevators[id];
    const float y = elevatorStatusY + static_cast<float>(id) * 54.0F;

    SDL_SetRenderDrawColor(renderer, 210, 217, 224, 255);
    SDL_RenderDebugTextFormat(renderer, PANEL_X + 18.0F, y, "E%zu  FLOOR %d",
                              id, elevator.currentFloor);
    SDL_RenderDebugTextFormat(
        renderer, PANEL_X + 18.0F, y + 18.0F, "%s  PENDING: %s",
        directionLabel(elevator.direction),
        elevator.hasPendingRequests ? "YES" : "NO");
    SDL_RenderDebugTextFormat(
        renderer, PANEL_X + 18.0F, y + 36.0F, "PAX: %zu / %zu",
        elevatorOccupancy(snapshot, id), elevator.capacity);
  }

  const auto upAssignment =
      snapshot.assignedElevator({uiState.selectedFloor, Direction::Up});
  const auto downAssignment =
      snapshot.assignedElevator({uiState.selectedFloor, Direction::Down});

  SDL_SetRenderDrawColor(renderer, 210, 217, 224, 255);
  const float assignmentY = context.simulationMode ? PANEL_Y + 300.0F
                                                   : PANEL_Y + 318.0F;
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
                      PANEL_Y + PANEL_HEIGHT - 96.0F, "/: MODE MENU");
  SDL_RenderDebugText(renderer, PANEL_X + 18.0F,
                      PANEL_Y + PANEL_HEIGHT - 78.0F,
                      "SPACE: PAUSE / RESUME");
  SDL_RenderDebugText(renderer, PANEL_X + 18.0F,
                      PANEL_Y + PANEL_HEIGHT - 60.0F,
                      "N: STEP WHEN PAUSED");
  SDL_RenderDebugText(renderer, PANEL_X + 18.0F,
                      PANEL_Y + PANEL_HEIGHT - 42.0F, "H / ?: HELP");
}

void renderModeMenuOverlay(SDL_Renderer *renderer, bool simulationMode) {
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, 4, 7, 10, 210);
  const SDL_FRect shade{0.0F, 0.0F,
                        static_cast<float>(rendering::WINDOW_WIDTH),
                        static_cast<float>(rendering::WINDOW_HEIGHT)};
  SDL_RenderFillRect(renderer, &shade);

  constexpr float MENU_WIDTH = 560.0F;
  constexpr float MENU_HEIGHT = 310.0F;
  constexpr float MENU_X = (rendering::WINDOW_WIDTH - MENU_WIDTH) / 2.0F;
  constexpr float MENU_Y = (rendering::WINDOW_HEIGHT - MENU_HEIGHT) / 2.0F;

  SDL_SetRenderDrawColor(renderer, 31, 41, 53, 255);
  const SDL_FRect menu{MENU_X, MENU_Y, MENU_WIDTH, MENU_HEIGHT};
  SDL_RenderFillRect(renderer, &menu);

  SDL_SetRenderDrawColor(renderer, 78, 190, 220, 255);
  SDL_RenderRect(renderer, &menu);
  SDL_RenderDebugText(renderer, MENU_X + 30.0F, MENU_Y + 28.0F,
                      "MODE SELECT");

  SDL_SetRenderDrawColor(renderer, 115, 128, 142, 255);
  SDL_RenderDebugTextFormat(renderer, MENU_X + 30.0F, MENU_Y + 58.0F,
                            "CURRENT: %s", modeLabel(simulationMode));

  const auto renderOption = [&](float y, const char *key, const char *label,
                                bool active) {
    if (active) {
      SDL_SetRenderDrawColor(renderer, 42, 75, 89, 255);
      const SDL_FRect selection{MENU_X + 24.0F, y - 12.0F,
                                MENU_WIDTH - 48.0F, 42.0F};
      SDL_RenderFillRect(renderer, &selection);
    }

    SDL_SetRenderDrawColor(renderer, active ? 78 : 225,
                           active ? 190 : 231, active ? 220 : 236, 255);
    SDL_RenderDebugText(renderer, MENU_X + 42.0F, y, key);
    SDL_RenderDebugText(renderer, MENU_X + 105.0F, y, label);
  };

  renderOption(MENU_Y + 118.0F, "1", "MANUAL MODE",
               !simulationMode);
  renderOption(MENU_Y + 174.0F, "2", "SIMULATION MODE",
               simulationMode);

  SDL_SetRenderDrawColor(renderer, 115, 128, 142, 255);
  SDL_RenderDebugText(renderer, MENU_X + 30.0F, MENU_Y + 250.0F,
                      "1 / 2: SWITCH MODE");
  SDL_RenderDebugText(renderer, MENU_X + 30.0F, MENU_Y + 270.0F,
                      "/ OR ESCAPE: CLOSE");

  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

void renderHelpOverlay(SDL_Renderer *renderer, bool simulationMode) {
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, 4, 7, 10, 205);
  const SDL_FRect shade{0.0F, 0.0F,
                        static_cast<float>(rendering::WINDOW_WIDTH),
                        static_cast<float>(rendering::WINDOW_HEIGHT)};
  SDL_RenderFillRect(renderer, &shade);

  constexpr float HELP_WIDTH = 650.0F;
  constexpr float HELP_HEIGHT = 460.0F;
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
  constexpr float ROW_SPACING = 34.0F;

  SDL_RenderDebugText(renderer, LABEL_X, y, "UP / DOWN ARROWS");
  SDL_RenderDebugText(renderer, DESCRIPTION_X, y,
                      simulationMode ? "SELECT ORIGIN FLOOR"
                                     : "SELECT FLOOR");
  y += ROW_SPACING;
  SDL_RenderDebugText(renderer, LABEL_X, y, "1 - 8");
  SDL_RenderDebugText(renderer, DESCRIPTION_X, y,
                      simulationMode ? "SELECT ORIGIN FLOOR"
                                     : "SELECT FLOOR");
  y += ROW_SPACING;

  if (!simulationMode) {
    SDL_RenderDebugText(renderer, LABEL_X, y, "LEFT / RIGHT ARROWS");
    SDL_RenderDebugText(renderer, DESCRIPTION_X, y, "SELECT ELEVATOR");
    y += ROW_SPACING;
    SDL_RenderDebugText(renderer, LABEL_X, y, "U / D");
    SDL_RenderDebugText(renderer, DESCRIPTION_X, y, "ADD HALL CALL");
    y += ROW_SPACING;
    SDL_RenderDebugText(renderer, LABEL_X, y, "E");
    SDL_RenderDebugText(renderer, DESCRIPTION_X, y,
                        "ADD INTERNAL REQUEST");
    y += ROW_SPACING;
  } else {
    SDL_RenderDebugText(renderer, LABEL_X, y, "U / D");
    SDL_RenderDebugText(renderer, DESCRIPTION_X, y,
                        "SPAWN UP / DOWN PASSENGER");
    y += ROW_SPACING;
  }

  SDL_RenderDebugText(renderer, LABEL_X, y, "SPACE");
  SDL_RenderDebugText(renderer, DESCRIPTION_X, y, "PAUSE / RESUME");
  y += ROW_SPACING;
  SDL_RenderDebugText(renderer, LABEL_X, y, "N");
  SDL_RenderDebugText(renderer, DESCRIPTION_X, y, "STEP WHILE PAUSED");
  y += ROW_SPACING;
  SDL_RenderDebugText(renderer, LABEL_X, y, "/");
  SDL_RenderDebugText(renderer, DESCRIPTION_X, y, "OPEN MODE MENU");
  y += ROW_SPACING;
  SDL_RenderDebugText(renderer, LABEL_X, y, "H / ?");
  SDL_RenderDebugText(renderer, DESCRIPTION_X, y, "TOGGLE HELP");
  y += ROW_SPACING;
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
                            const SimulationSnapshot &snapshot,
                            bool simulationMode,
                            bool showModeMenu) {
  const RenderContext context{renderer, uiState, snapshot, simulationMode,
                              calculateShaftWidth(snapshot.elevators.size())};

  SDL_SetRenderDrawColor(renderer, 16, 21, 29, 255);
  SDL_RenderClear(renderer);

  renderBuilding(context);
  renderElevators(context);
  renderPassengers(context);
  renderStatusPanel(context);

  if (uiState.showHelp) {
    renderHelpOverlay(renderer, simulationMode);
  }

  if (showModeMenu) {
    renderModeMenuOverlay(renderer, simulationMode);
  }

  SDL_RenderPresent(renderer);
}
