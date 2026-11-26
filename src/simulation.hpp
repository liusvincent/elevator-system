#pragma once

#include "dispatcher.hpp"
#include "passenger.hpp"

#include <array>
#include <cstdint>
#include <deque>
#include <optional>
#include <vector>

struct PassengerSnapshot {
  PassengerId id;
  int originFloor;
  int destinationFloor;
  PassengerState state;
  std::optional<ElevatorId> elevatorId;
  std::uint64_t spawnedAtTick;
  std::optional<std::uint64_t> boardedAtTick;
  std::optional<std::uint64_t> arrivedAtTick;
};

struct SimulationSnapshot {
  std::uint64_t currentTick;
  std::vector<ElevatorSnapshot> elevators;
  std::vector<HallCallAssignment> hallCallAssignments;
  std::vector<PassengerSnapshot> passengers;

  std::optional<ElevatorId> assignedElevator(HallCall call) const;
};

class Simulation {
public:
  explicit Simulation(int elevatorCount);

  PassengerId spawnPassenger(int originFloor, int destinationFloor);

  void addHallCall(HallCall call);
  void selectFloor(ElevatorId elevatorId, int floor);

  std::vector<DispatcherServiceEvent> tick();

  SimulationSnapshot snapshot() const;
  std::size_t elevatorCount() const;

private:
  using PassengerQueue = std::deque<PassengerId>;

  struct WaitingQueue {
    PassengerQueue passengers;
    std::optional<ElevatorId> awaitingDepartureOf;
  };

  std::vector<ElevatorSnapshot> elevatorSnapshots() const;
  WaitingQueue &waitingQueue(int floor, Direction direction);
  void reactivateWaitingQueues(
      const std::vector<ElevatorSnapshot> &elevators);
  void reactivateWaitingQueue(
      WaitingQueue &queue, HallCall call,
      const std::vector<ElevatorSnapshot> &elevators);
  void servicePassengers(ElevatorId elevatorId,
                         const ServiceEvent &service);
  void unloadPassengers(ElevatorId elevatorId, int floor);
  void boardPassengers(ElevatorId elevatorId, int floor,
                       Direction direction);

  std::uint64_t currentTick_ = 0;
  Dispatcher dispatcher_;
  std::vector<Passenger> passengers_;
  std::array<WaitingQueue, MAX_FLOOR + 1> waitingUp_;
  std::array<WaitingQueue, MAX_FLOOR + 1> waitingDown_;
  std::vector<std::vector<PassengerId>> elevatorPassengers_;
};
