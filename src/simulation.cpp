#include "simulation.hpp"

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

Simulation::Simulation(int elevatorCount) : dispatcher_(elevatorCount) {}

void Simulation::addHallCall(HallCall call) {
  dispatcher_.addHallCall(call);
}

void Simulation::selectFloor(ElevatorId elevatorId, int floor) {
  dispatcher_.selectFloor(elevatorId, floor);
}

std::vector<DispatcherServiceEvent> Simulation::tick() {
  std::vector<DispatcherServiceEvent> events = dispatcher_.tick();
  ++currentTick_;
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

  return result;
}

std::size_t Simulation::elevatorCount() const {
  return dispatcher_.elevatorCount();
}
