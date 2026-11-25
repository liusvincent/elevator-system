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
  if ((call.floor == MIN_FLOOR && call.direction == Direction::Down) ||
      (call.floor == MAX_FLOOR && call.direction == Direction::Up)) {
    throw std::invalid_argument(
        "Hall call direction is invalid for this floor");
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

std::vector<DispatcherServiceEvent> Dispatcher::tick() {
  std::vector<DispatcherServiceEvent> events;

  for (ElevatorId id = 0; id < elevators_.size(); ++id) {
    const std::optional<ServiceEvent> service = elevators_[id].tick();
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

std::vector<HallCallAssignment> Dispatcher::hallCallAssignments() const {
  std::vector<HallCallAssignment> result;
  result.reserve(assignments_.size());

  for (const auto &[call, elevatorId] : assignments_) {
    result.push_back({{call.first, call.second}, elevatorId});
  }

  return result;
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
  int bestTicks = std::numeric_limits<int>::max();

  for (ElevatorId id = 0; id < elevators_.size(); ++id) {
    const int ticks = estimatePickupTicks(elevators_[id], call);
    if (ticks < bestTicks) {
      bestTicks = ticks;
      bestElevator = id;
    }
  }

  return bestElevator;
}

int Dispatcher::estimatePickupTicks(const Elevator &elevator,
                                    HallCall call) const {
  Elevator candidate = elevator;
  candidate.assignHallCall(call);

  constexpr int MAX_PICKUP_ESTIMATE_TICKS = 10'000;

  for (int ticks = 1; ticks <= MAX_PICKUP_ESTIMATE_TICKS; ++ticks) {
    const std::optional<ServiceEvent> service = candidate.tick();

    if (service && service->floor == call.floor &&
        service->servedHallCall == call.direction) {
      return ticks;
    }
  }

  return std::numeric_limits<int>::max();
}
