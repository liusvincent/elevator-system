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

struct HallCallAssignment {
  HallCall call;
  ElevatorId elevatorId;
};

class Dispatcher {
public:
  explicit Dispatcher(int elevatorCount);

  void addHallCall(HallCall call);
  void selectFloor(ElevatorId elevatorId, int floor);

  std::vector<DispatcherServiceEvent> tick();

  std::size_t elevatorCount() const;
  std::size_t elevatorCapacity(ElevatorId elevatorId) const;
  ElevatorSnapshot elevatorSnapshot(ElevatorId elevatorId) const;
  std::vector<HallCallAssignment> hallCallAssignments() const;
  std::optional<ElevatorId> assignedElevator(HallCall call) const;

private:
  ElevatorId chooseElevator(HallCall call) const;
  int estimatePickupTicks(const Elevator &elevator, HallCall call) const;

  std::vector<Elevator> elevators_;
  std::map<std::pair<int, Direction>, ElevatorId> assignments_;
};
