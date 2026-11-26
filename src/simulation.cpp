#include "simulation.hpp"

#include <algorithm>
#include <stdexcept>

std::optional<ElevatorId>
SimulationSnapshot::assignedElevator(HallCall call) const {
  for (const HallCallAssignment &assignment : hallCallAssignments) {
    if (assignment.call.floor == call.floor &&
        assignment.call.direction == call.direction) {
      return assignment.elevatorId;
    }
  }

  return std::nullopt;
}

Simulation::Simulation(int elevatorCount)
    : dispatcher_(elevatorCount), elevatorPassengers_(elevatorCount) {}

PassengerId Simulation::spawnPassenger(int originFloor,
                                       int destinationFloor) {
  const PassengerId id = passengers_.size();
  passengers_.emplace_back(id, originFloor, destinationFloor, currentTick_);

  Passenger &passenger = passengers_.back();
  WaitingQueue &queue =
      waitingQueue(passenger.originFloor(), passenger.direction());
  const bool needsHallCall =
      queue.passengers.empty() && !queue.awaitingDepartureOf;
  queue.passengers.push_back(id);

  if (needsHallCall) {
    dispatcher_.addHallCall(
        {passenger.originFloor(), passenger.direction()});
  }

  return id;
}

void Simulation::addHallCall(HallCall call) {
  dispatcher_.addHallCall(call);
}

void Simulation::selectFloor(ElevatorId elevatorId, int floor) {
  dispatcher_.selectFloor(elevatorId, floor);
}

std::vector<DispatcherServiceEvent> Simulation::tick() {
  std::vector<DispatcherServiceEvent> events = dispatcher_.tick();
  ++currentTick_;

  for (const DispatcherServiceEvent &event : events) {
    servicePassengers(event.elevatorId, event.service);
  }

  const std::vector<ElevatorSnapshot> after = elevatorSnapshots();
  reactivateWaitingQueues(after);

  return events;
}

SimulationSnapshot Simulation::snapshot() const {
  SimulationSnapshot result;
  result.currentTick = currentTick_;
  result.elevators.reserve(dispatcher_.elevatorCount());

  for (ElevatorId id = 0; id < dispatcher_.elevatorCount(); ++id) {
    result.elevators.push_back(dispatcher_.elevatorSnapshot(id));
  }

  result.hallCallAssignments = dispatcher_.hallCallAssignments();

  std::vector<std::optional<ElevatorId>> passengerElevators(
      passengers_.size());
  for (ElevatorId elevatorId = 0;
       elevatorId < elevatorPassengers_.size(); ++elevatorId) {
    for (PassengerId passengerId : elevatorPassengers_[elevatorId]) {
      passengerElevators[passengerId] = elevatorId;
    }
  }

  result.passengers.reserve(passengers_.size());
  for (const Passenger &passenger : passengers_) {
    result.passengers.push_back(
        {passenger.id(), passenger.originFloor(), passenger.destinationFloor(),
         passenger.state(), passengerElevators[passenger.id()],
         passenger.spawnedAtTick(), passenger.boardedAtTick(),
         passenger.arrivedAtTick()});
  }

  return result;
}

std::size_t Simulation::elevatorCount() const {
  return dispatcher_.elevatorCount();
}

std::vector<ElevatorSnapshot> Simulation::elevatorSnapshots() const {
  std::vector<ElevatorSnapshot> snapshots;
  snapshots.reserve(dispatcher_.elevatorCount());

  for (ElevatorId id = 0; id < dispatcher_.elevatorCount(); ++id) {
    snapshots.push_back(dispatcher_.elevatorSnapshot(id));
  }

  return snapshots;
}

Simulation::WaitingQueue &Simulation::waitingQueue(int floor,
                                                   Direction direction) {
  if (direction == Direction::Up) {
    return waitingUp_.at(static_cast<std::size_t>(floor));
  }
  if (direction == Direction::Down) {
    return waitingDown_.at(static_cast<std::size_t>(floor));
  }

  throw std::invalid_argument("Passenger queue direction cannot be idle");
}

void Simulation::reactivateWaitingQueues(
    const std::vector<ElevatorSnapshot> &elevators) {
  for (int floor = MIN_FLOOR; floor <= MAX_FLOOR; ++floor) {
    if (floor < MAX_FLOOR) {
      reactivateWaitingQueue(waitingUp_[floor],
                             {floor, Direction::Up}, elevators);
    }
    if (floor > MIN_FLOOR) {
      reactivateWaitingQueue(waitingDown_[floor],
                             {floor, Direction::Down}, elevators);
    }
  }
}

void Simulation::reactivateWaitingQueue(
    WaitingQueue &queue, HallCall call,
    const std::vector<ElevatorSnapshot> &elevators) {
  if (!queue.awaitingDepartureOf) {
    return;
  }

  const ElevatorId elevatorId = *queue.awaitingDepartureOf;
  const ElevatorSnapshot &elevator = elevators.at(elevatorId);
  const bool hasDeparted = elevator.state == ElevatorState::Moving;
  const bool isAvailableAgain = elevator.state == ElevatorState::Idle &&
                                !elevator.hasPendingRequests;

  if (!hasDeparted && !isAvailableAgain) {
    return;
  }

  queue.awaitingDepartureOf = std::nullopt;
  if (!queue.passengers.empty()) {
    dispatcher_.addHallCall(call);
  }
}

void Simulation::servicePassengers(ElevatorId elevatorId,
                                   const ServiceEvent &service) {
  unloadPassengers(elevatorId, service.floor);

  if (service.servedHallCall) {
    boardPassengers(elevatorId, service.floor,
                    *service.servedHallCall);
  }
}

void Simulation::unloadPassengers(ElevatorId elevatorId, int floor) {
  std::vector<PassengerId> &occupants = elevatorPassengers_[elevatorId];

  const auto arrived = std::remove_if(
      occupants.begin(), occupants.end(), [&](PassengerId passengerId) {
        Passenger &passenger = passengers_[passengerId];
        if (passenger.destinationFloor() != floor) {
          return false;
        }

        passenger.arrive(currentTick_);
        return true;
      });

  occupants.erase(arrived, occupants.end());
}

void Simulation::boardPassengers(ElevatorId elevatorId, int floor,
                                 Direction direction) {
  WaitingQueue &queue = waitingQueue(floor, direction);
  std::vector<PassengerId> &occupants = elevatorPassengers_[elevatorId];

  while (!queue.passengers.empty() &&
         occupants.size() < dispatcher_.elevatorCapacity(elevatorId)) {
    const PassengerId passengerId = queue.passengers.front();
    queue.passengers.pop_front();

    Passenger &passenger = passengers_[passengerId];
    passenger.board(currentTick_);
    occupants.push_back(passengerId);
    dispatcher_.selectFloor(elevatorId, passenger.destinationFloor());
  }

  queue.awaitingDepartureOf = elevatorId;
}
