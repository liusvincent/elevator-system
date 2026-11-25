#pragma once

#include "elevator.hpp"

#include <cstddef>
#include <map>
#include <optional>
#include <utility>
#include <vector>

using ElevatorId = std::size_t;

struct DispatcherServiceEvent {
  ElevatorId elevatorId;
  ServiceEvent service;
};

class Dispatcher {
public:
  explicit Dispatcher(int elevatorCount);

  void addHallCall(HallCall call);
  void selectFloor(ElevatorId elevatorId, int floor);

  std::vector<DispatcherServiceEvent> advance();

  std::size_t elevatorCount() const;
  ElevatorSnapshot elevatorSnapshot(ElevatorId elevatorId) const;
  std::optional<ElevatorId> assignedElevator(HallCall call) const;

private:
  ElevatorId chooseElevator(HallCall call) const;
  int estimateSteps(const Elevator &elevator, HallCall call) const;

  std::vector<Elevator> elevators_;
  std::map<std::pair<int, Direction>, ElevatorId> assignments_;
};
