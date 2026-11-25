#include "simulation.hpp"
#include "simulation_timing.hpp"

#include <cmath>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

void expect(bool condition, std::string_view message) {
  if (!condition) {
    throw std::runtime_error(std::string(message));
  }
}

template <typename TestFunction>
bool runTest(std::string_view name, TestFunction test) {
  try {
    test();
    std::cout << "[PASS] " << name << '\n';
    return true;
  } catch (const std::exception &error) {
    std::cerr << "[FAIL] " << name << ": " << error.what() << '\n';
    return false;
  } catch (...) {
    std::cerr << "[FAIL] " << name << ": unknown exception\n";
    return false;
  }
}

void testStartsAtTickZeroWithEntireFleet() {
  const Simulation simulation{3};
  const SimulationSnapshot snapshot = simulation.snapshot();

  expect(snapshot.currentTick == 0, "Simulation should start at tick zero");
  expect(snapshot.elevators.size() == 3,
         "Snapshot should contain every elevator");

  for (const ElevatorSnapshot &elevator : snapshot.elevators) {
    expect(elevator.currentFloor == MIN_FLOOR,
           "Every elevator should start at the bottom floor");
    expect(elevator.state == ElevatorState::Idle,
           "Every elevator should start idle");
  }
}

void testTickAdvancesWorldCounter() {
  Simulation simulation{1};

  simulation.tick();

  expect(simulation.snapshot().currentTick == 1,
         "One simulation tick should advance the world counter once");
}

void testForwardsInternalRequest() {
  Simulation simulation{1};

  simulation.selectFloor(0, 3);
  simulation.tick();

  const ElevatorSnapshot elevator = simulation.snapshot().elevators.at(0);
  expect(elevator.state == ElevatorState::Moving,
         "Internal request should start the elevator moving");
  expect(elevator.currentFloor == MIN_FLOOR,
         "One tick should not move the elevator a full floor");
  expect(elevator.hasPendingRequests,
         "Internal request should remain pending while travelling");
}

void testAssignsHallCall() {
  Simulation simulation{1};
  const HallCall call{3, Direction::Up};

  simulation.addHallCall(call);

  const SimulationSnapshot snapshot = simulation.snapshot();

  expect(snapshot.hallCallAssignments.size() == 1,
         "Snapshot should contain the active hall-call assignment");
  expect(snapshot.assignedElevator(call) == ElevatorId{0},
         "Snapshot should expose the assigned elevator");
}

void testPropagatesServiceEvent() {
  Simulation simulation{1};
  const HallCall call{MIN_FLOOR, Direction::Up};

  simulation.addHallCall(call);
  const std::vector<DispatcherServiceEvent> events = simulation.tick();

  expect(events.size() == 1,
         "Servicing the current floor should produce one event");
  expect(events.front().elevatorId == ElevatorId{0},
         "Service event should identify the elevator");
  expect(events.front().service.floor == MIN_FLOOR,
         "Service event should identify the serviced floor");
  expect(events.front().service.servedHallCall == Direction::Up,
         "Service event should identify the hall-call direction");
  expect(!simulation.snapshot().assignedElevator(call).has_value(),
         "Serviced hall call should no longer be assigned");
}

void testSameInputsProduceSameState() {
  Simulation first{2};
  Simulation second{2};
  const HallCall call{3, Direction::Up};

  first.selectFloor(0, 5);
  second.selectFloor(0, 5);
  first.addHallCall(call);
  second.addHallCall(call);

  const int ticks = 2 * simulation_timing::TRAVEL_TICKS_PER_FLOOR + 1;
  for (int i = 0; i < ticks; ++i) {
    first.tick();
    second.tick();
  }

  const SimulationSnapshot firstSnapshot = first.snapshot();
  const SimulationSnapshot secondSnapshot = second.snapshot();

  expect(firstSnapshot.currentTick == secondSnapshot.currentTick,
         "Equal inputs should produce equal tick counters");
  expect(firstSnapshot.elevators.size() == secondSnapshot.elevators.size(),
         "Equal simulations should contain the same fleet size");

  for (std::size_t id = 0; id < firstSnapshot.elevators.size(); ++id) {
    const ElevatorSnapshot &left = firstSnapshot.elevators[id];
    const ElevatorSnapshot &right = secondSnapshot.elevators[id];

    expect(left.currentFloor == right.currentFloor,
           "Equal inputs should produce equal elevator floors");
    expect(left.direction == right.direction,
           "Equal inputs should produce equal elevator directions");
    expect(left.state == right.state,
           "Equal inputs should produce equal elevator states");
    expect(std::abs(left.stateProgress - right.stateProgress) < 0.0001F,
           "Equal inputs should produce equal state progress");
    expect(left.hasPendingRequests == right.hasPendingRequests,
           "Equal inputs should produce equal pending-request state");
  }
}

} // namespace

int main() {
  int failures = 0;

  failures += !runTest("starts at tick zero with entire fleet",
                       testStartsAtTickZeroWithEntireFleet);
  failures += !runTest("tick advances world counter",
                       testTickAdvancesWorldCounter);
  failures += !runTest("forwards internal request",
                       testForwardsInternalRequest);
  failures += !runTest("assigns hall call", testAssignsHallCall);
  failures += !runTest("propagates service event",
                       testPropagatesServiceEvent);
  failures += !runTest("same inputs produce same state",
                       testSameInputsProduceSameState);

  if (failures == 0) {
    std::cout << "All simulation tests passed.\n";
  }

  return failures == 0 ? 0 : 1;
}
