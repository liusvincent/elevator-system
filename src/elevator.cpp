#include "elevator.hpp"

#include "simulation_timing.hpp"

#include <cstdlib>
#include <limits>
#include <stdexcept>

Elevator::Elevator()
    : currentFloor_(MIN_FLOOR), direction_(Direction::Idle),
      state_(ElevatorState::Idle), stateTicksElapsed_(0) {}

void Elevator::addInternalRequest(int floor) {
  if (floor < MIN_FLOOR || floor > MAX_FLOOR) {
    throw std::out_of_range("Invalid floor request: out of floor range");
  }

  internalRequests_.insert(floor);
}

void Elevator::assignHallCall(HallCall call) {
  if (call.floor < MIN_FLOOR || call.floor > MAX_FLOOR) {
    throw std::out_of_range("Invalid hall call: out of floor range");
  }
  if ((call.floor == MIN_FLOOR && call.direction == Direction::Down) ||
      (call.floor == MAX_FLOOR && call.direction == Direction::Up)) {
    throw std::invalid_argument("Invalid hall call: floor bounds reached");
  }
  if (call.direction == Direction::Idle) {
    throw std::invalid_argument("Invalid hall call: direction cannot be idle");
  }

  if (call.direction == Direction::Up) {
    upHallCalls_.insert(call.floor);
  } else {
    downHallCalls_.insert(call.floor);
  }
}

std::optional<ServiceEvent> Elevator::tick() {
  switch (state_) {
  case ElevatorState::Idle:
    return chooseNextAction();

  case ElevatorState::Moving:
    return advanceMovement();

  case ElevatorState::DoorOpening:
  case ElevatorState::DoorOpen:
  case ElevatorState::DoorClosing:
    advanceDoors();
    return std::nullopt;
  }

  return std::nullopt;
}

ElevatorSnapshot Elevator::snapshot() const {
  return {currentFloor_, direction_, state_, stateProgress(),
          hasPendingRequests()};
}

bool Elevator::hasPendingRequests() const {
  return !internalRequests_.empty() || !upHallCalls_.empty() ||
         !downHallCalls_.empty();
}

std::optional<ServiceEvent> Elevator::chooseNextAction() {
  if (direction_ == Direction::Idle) {
    chooseDirection();
  }

  if (hasRequestAtCurrentFloor()) {
    return beginService();
  }

  if (!hasRequestInCurrentDirection()) {
    chooseDirection();

    if (hasRequestAtCurrentFloor()) {
      return beginService();
    }
  }

  if (direction_ == Direction::Idle) {
    return std::nullopt;
  }

  beginMoving();
  return std::nullopt;
}

std::optional<ServiceEvent> Elevator::advanceMovement() {
  ++stateTicksElapsed_;

  if (stateTicksElapsed_ <
      simulation_timing::TRAVEL_TICKS_PER_FLOOR) {
    return std::nullopt;
  }

  if (direction_ == Direction::Up && currentFloor_ < MAX_FLOOR) {
    ++currentFloor_;
  } else if (direction_ == Direction::Down && currentFloor_ > MIN_FLOOR) {
    --currentFloor_;
  } else {
    direction_ = Direction::Idle;
  }

  stateTicksElapsed_ = 0;
  state_ = ElevatorState::Idle;
  return chooseNextAction();
}

std::optional<ServiceEvent> Elevator::beginService() {
  ServiceEvent event{currentFloor_, false, std::nullopt};

  if (internalRequests_.erase(currentFloor_) > 0) {
    event.servedInternalRequest = true;
  }

  if (direction_ == Direction::Up && upHallCalls_.erase(currentFloor_) > 0) {
    event.servedHallCall = Direction::Up;
  } else if (direction_ == Direction::Down &&
             downHallCalls_.erase(currentFloor_) > 0) {
    event.servedHallCall = Direction::Down;
  }

  if (!event.servedInternalRequest && !event.servedHallCall) {
    return std::nullopt;
  }

  state_ = ElevatorState::DoorOpening;
  stateTicksElapsed_ = 0;
  return event;
}

void Elevator::beginMoving() {
  const bool canMoveUp =
      direction_ == Direction::Up && currentFloor_ < MAX_FLOOR;
  const bool canMoveDown =
      direction_ == Direction::Down && currentFloor_ > MIN_FLOOR;

  if (!canMoveUp && !canMoveDown) {
    direction_ = Direction::Idle;
    state_ = ElevatorState::Idle;
    return;
  }

  state_ = ElevatorState::Moving;
  stateTicksElapsed_ = 0;
}

void Elevator::advanceDoors() {
  ++stateTicksElapsed_;

  switch (state_) {
  case ElevatorState::DoorOpening:
    if (stateTicksElapsed_ >= simulation_timing::DOOR_OPENING_TICKS) {
      state_ = ElevatorState::DoorOpen;
      stateTicksElapsed_ = 0;
    }
    break;

  case ElevatorState::DoorOpen:
    if (stateTicksElapsed_ >= simulation_timing::DOOR_OPEN_TICKS) {
      state_ = ElevatorState::DoorClosing;
      stateTicksElapsed_ = 0;
    }
    break;

  case ElevatorState::DoorClosing:
    if (stateTicksElapsed_ >= simulation_timing::DOOR_CLOSING_TICKS) {
      state_ = ElevatorState::Idle;
      stateTicksElapsed_ = 0;
    }
    break;

  case ElevatorState::Idle:
  case ElevatorState::Moving:
    break;
  }
}

void Elevator::chooseDirection() {
  if (!hasPendingRequests()) {
    direction_ = Direction::Idle;
    return;
  }

  if (upHallCalls_.contains(currentFloor_)) {
    direction_ = Direction::Up;
    return;
  }
  if (downHallCalls_.contains(currentFloor_)) {
    direction_ = Direction::Down;
    return;
  }

  int closestDistance = std::numeric_limits<int>::max();
  int closestFloor = std::numeric_limits<int>::max();

  const auto considerRequests = [&](const std::set<int> &requests) {
    for (int floor : requests) {
      const int distance = std::abs(currentFloor_ - floor);
      if (distance < closestDistance ||
          (distance == closestDistance && floor < closestFloor)) {
        closestDistance = distance;
        closestFloor = floor;
      }
    }
  };

  considerRequests(internalRequests_);
  considerRequests(upHallCalls_);
  considerRequests(downHallCalls_);

  if (closestFloor > currentFloor_) {
    direction_ = Direction::Up;
  } else if (closestFloor < currentFloor_) {
    direction_ = Direction::Down;
  } else {
    direction_ = Direction::Idle;
  }
}

bool Elevator::hasRequestAtCurrentFloor() const {
  if (internalRequests_.contains(currentFloor_)) {
    return true;
  }
  if (direction_ == Direction::Up && upHallCalls_.contains(currentFloor_)) {
    return true;
  }
  if (direction_ == Direction::Down &&
      downHallCalls_.contains(currentFloor_)) {
    return true;
  }

  return false;
}

bool Elevator::hasRequestAbove() const {
  return internalRequests_.upper_bound(currentFloor_) !=
             internalRequests_.end() ||
         upHallCalls_.upper_bound(currentFloor_) != upHallCalls_.end() ||
         downHallCalls_.upper_bound(currentFloor_) != downHallCalls_.end();
}

bool Elevator::hasRequestBelow() const {
  return (!internalRequests_.empty() &&
          *internalRequests_.begin() < currentFloor_) ||
         (!upHallCalls_.empty() && *upHallCalls_.begin() < currentFloor_) ||
         (!downHallCalls_.empty() &&
          *downHallCalls_.begin() < currentFloor_);
}

bool Elevator::hasRequestInCurrentDirection() const {
  if (direction_ == Direction::Up) {
    return hasRequestAbove();
  }
  if (direction_ == Direction::Down) {
    return hasRequestBelow();
  }

  return false;
}

float Elevator::stateProgress() const {
  switch (state_) {
  case ElevatorState::Moving:
    return static_cast<float>(stateTicksElapsed_) /
           simulation_timing::TRAVEL_TICKS_PER_FLOOR;

  case ElevatorState::DoorOpening:
    return static_cast<float>(stateTicksElapsed_) /
           simulation_timing::DOOR_OPENING_TICKS;

  case ElevatorState::DoorOpen:
    return static_cast<float>(stateTicksElapsed_) /
           simulation_timing::DOOR_OPEN_TICKS;

  case ElevatorState::DoorClosing:
    return static_cast<float>(stateTicksElapsed_) /
           simulation_timing::DOOR_CLOSING_TICKS;

  case ElevatorState::Idle:
    return 0.0F;
  }

  return 0.0F;
}
