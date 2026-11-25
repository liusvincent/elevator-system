#include "dispatcher.hpp"

#include <limits>
#include <stdexcept>

Dispatcher::Dispatcher(int elevatorCount) {
  if (elevatorCount <= 0) {
    throw std::invalid_argument(
        "Dispatcher requires at least one elevator");
  }

  elevators_.resize(static_cast<std::size_t>(elevatorCount));
}

void Dispatcher::addHallCall(HallCall call) {
  if (call.floor < MIN_FLOOR || call.floor > MAX_FLOOR) {
    throw std::out_of_range("Hall call is outside the building");
  }
  if (call.direction == Direction::Idle) {
    throw std::invalid_argument("Hall call direction cannot be idle");
  }

  const auto key = std::pair{call.floor, call.direction};
  if (assignments_.contains(key)) {
    return;
  }

  const ElevatorId elevatorId = chooseElevator(call);
  elevators_[elevatorId].assignHallCall(call);
  assignments_[key] = elevatorId;
}

void Dispatcher::selectFloor(ElevatorId elevatorId, int floor) {
  elevators_.at(elevatorId).addInternalRequest(floor);
}

std::vector<DispatcherServiceEvent> Dispatcher::advance() {
  std::vector<DispatcherServiceEvent> events;

  for (ElevatorId id = 0; id < elevators_.size(); ++id) {
    const std::optional<ServiceEvent> service = elevators_[id].step();
    if (!service) {
      continue;
    }

    if (service->servedHallCall) {
      const auto key =
          std::pair{service->floor, *service->servedHallCall};
      const auto assignment = assignments_.find(key);

      if (assignment != assignments_.end() && assignment->second == id) {
        assignments_.erase(assignment);
      }
    }

    events.push_back({id, *service});
  }

  return events;
}

std::size_t Dispatcher::elevatorCount() const { return elevators_.size(); }

ElevatorSnapshot
Dispatcher::elevatorSnapshot(ElevatorId elevatorId) const {
  return elevators_.at(elevatorId).snapshot();
}

std::optional<ElevatorId>
Dispatcher::assignedElevator(HallCall call) const {
  const auto assignment = assignments_.find({call.floor, call.direction});
  if (assignment == assignments_.end()) {
    return std::nullopt;
  }

  return assignment->second;
}

ElevatorId Dispatcher::chooseElevator(HallCall call) const {
  ElevatorId bestElevator = 0;
  int bestSteps = std::numeric_limits<int>::max();

  for (ElevatorId id = 0; id < elevators_.size(); ++id) {
    const int steps = estimateSteps(elevators_[id], call);
    if (steps < bestSteps) {
      bestSteps = steps;
      bestElevator = id;
    }
  }

  return bestElevator;
}

int Dispatcher::estimateSteps(const Elevator &elevator, HallCall call) const {
  Elevator candidate = elevator;
  candidate.assignHallCall(call);

  constexpr int maximumSteps = 100;

  for (int steps = 1; steps <= maximumSteps; ++steps) {
    const std::optional<ServiceEvent> service = candidate.step();

    if (service && service->floor == call.floor &&
        service->servedHallCall == call.direction) {
      return steps;
    }
  }

  return std::numeric_limits<int>::max();
}
