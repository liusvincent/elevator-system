#pragma once

#include "dispatcher.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

struct SimulationSnapshot {
  std::uint64_t currentTick;
  std::vector<ElevatorSnapshot> elevators;
  std::vector<HallCallAssignment> hallCallAssignments;

  std::optional<ElevatorId> assignedElevator(HallCall call) const;
};

class Simulation {
public:
  explicit Simulation(int elevatorCount);

  void addHallCall(HallCall call);
  void selectFloor(ElevatorId elevatorId, int floor);

  std::vector<DispatcherServiceEvent> tick();

  SimulationSnapshot snapshot() const;
  std::size_t elevatorCount() const;

private:
  std::uint64_t currentTick_ = 0;
  Dispatcher dispatcher_;
};
